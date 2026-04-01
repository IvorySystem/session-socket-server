/*------------------------------------------------------------------------------
 *
 * (c) Copyright 2000-2026 by Alasdair Scott
 *
 * Part of the Ivory System/Script project
 *
 * https://ivorysystem.net | https://ivoryscript.net
 * https://github.com/IvorySystem | https://github.com/IvoryScript
 *
 * Name:    server.cpp
 *
 * Module:  Ivory session socket server
 *
 * Author:  Alasdair Scott
 *
 * Original date: August 2019
 *
 *------------------------------------------------------------------------------
 *
 * Description:
 *
 *    Server code for multiple client sockets originally based on
 *    https://www.ibm.com/support/knowledgecenter/en/ssw_ibm_i_74/rzab6/poll.htm
 *
 * Modification history:
 *
 *------------------------------------------------------------------------------
 *
 * License: MIT
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 *
 *-----------------------------------------------------------------------------
 */

#include <arpa/inet.h>
#include <chrono>
#include <cstdlib>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/prctl.h>
#include <sys/signalfd.h>
#include <sys/socket.h>
#include <sys/timerfd.h>
#include <sys/types.h>
#include <sys/wait.h>

#define GEN_BUF_SIZE 2048

#define trace1(fmt,arg)\
{char traceBuf[GEN_BUF_SIZE];\
sprintf(traceBuf,(fmt),(arg));\
::trace(traceBuf);}

#define LISTEN_POLL_INDEX     0
#define SIGNAL_POLL_INDEX     1
#define TIMER_POLL_INDEX      2
#define N_BASE_FDS            3

#define MAX_COMMAND_LEN       8192

#define TIMER_PERIOD          60    // 1 minute timer
//#define TIMER_PERIOD          5    // 5s timer

#define START_SESSION         "START SESSION"
#define SESSION               "SESSION"
#define SEND                  "SEND"
#define RECEIVE               "RECEIVE"
#define STATUS                "STATUS"
#define END                   "END"

#define PROMPT                "> "
#define STARTED               "STARTED: "
#define SENT                  "SENT\n"
#define RECEIVED              "RECEIVED: "
#define READY                 "READY\n"
#define ENDED                 "ENDED\n"
#define ERROR                 "ERROR: "
#define ERROR_FAILED          "ERROR: FAILED\n"
#define TIMEOUT               "TIMEOUT\n"

#define UNKNOWN_SESSION_ID    "UNKNOWN SESSION ID"

#define NEWLINE               "\n"

#include "server.h"

int doCommand(char* command, int len, int client) {
   return len;
}

#define DEFAULT_BACKLOG    5
#define MAX_POLL_FDS       100
#define MAX_CLIENTS        4
#define RECV_BUFFER_LEN    4096
#define READ_BUFFER_LEN    4096

#define MAX_SEND_LEN      4096


extern void error(const char* str);

extern void trace(const char* str);

int setReuseAddr(int fd) {
   int reuseAddr = 1;
   return setsockopt(fd, SOL_SOCKET, SO_REUSEADDR,
                     &reuseAddr, sizeof(reuseAddr)) >= 0;
}

int setNonBlocking(int fd) {
   return fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK) >= 0;
}


void reportError(int fd, const char* msg) {
   ::send(fd, ERROR, strlen(ERROR), MSG_NOSIGNAL);
   ::send(fd, msg, strlen(msg), MSG_NOSIGNAL);
   ::send(fd, NEWLINE, strlen(NEWLINE), MSG_NOSIGNAL);
}

PendingData::PendingData(void)
   : _buffer(NULL),
     _bufferSize(0),
     _bufferLen(0) {
}

void PendingData::add(const char* data, unsigned int len) {
   if (_bufferLen + len + 1 > _bufferSize) {
      _bufferSize = _bufferLen + len + 1;
      char* buffer = new char[_bufferSize];
      memcpy(buffer, _buffer, _bufferLen);
      delete[] _buffer;
      _buffer = buffer;
   }
   memcpy(_buffer + _bufferLen, data, len);
   _bufferLen += len;    
}

void PendingData::remove(unsigned int len) {
   if (len >= _bufferLen)
      _bufferLen = 0;
   else {
      memmove(_buffer, _buffer + len, _bufferLen - len);
      _bufferLen -= len;
   }
}

Worker::Worker(unsigned long sessionId)
 : _sessionId(sessionId), _pid(-1),
   _rcvdCmd(FALSE), _lastAccessTime(::time(NULL)),
   _timeout(FALSE) {
   _pipeFD[0][0] = -1;
   _pipeFD[0][1] = -1;
   _pipeFD[1][0] = -1;
   _pipeFD[1][1] = -1;
   _pipeFD[2][0] = -1;
   _pipeFD[2][1] = -1;
}

bool Worker::createPipes(void) {
   if (::pipe(_pipeFD[0]) != 0 ||
       ::pipe(_pipeFD[1]) != 0 ||
       ::pipe(_pipeFD[2]) != 0)
      return FALSE;
   fcntl(_pipeFD[0][1], F_SETFL, O_NONBLOCK);
   fcntl(_pipeFD[1][0], F_SETFL, O_NONBLOCK);
   fcntl(_pipeFD[2][0], F_SETFL, O_NONBLOCK);
   return TRUE;
}

void Worker::closePipes(void) {
   ::close(_pipeFD[0][1]);
   ::close(_pipeFD[1][0]);
   ::close(_pipeFD[2][0]);
}

void Worker::endSession(void) {
   ::kill(_pid, SIGTERM);
   _sessionId = 0;
}

Client::Client(int fd)
 : _fd(fd),
   _rc(0),
   _ackSent(FALSE),
   _recvEnable(TRUE),
   _worker(NULL) {
}

bool Client::send(const char* buf, unsigned int len) {
   _pendingOutput.add(buf, len);
   unsigned int sendLen = _pendingOutput.bufferLen();
   if (sendLen == 0)
      return true;
   if (sendLen > MAX_SEND_LEN)
      sendLen = MAX_SEND_LEN;
   ssize_t sent = ::send(_fd, _pendingOutput.buffer(), sendLen, MSG_NOSIGNAL);
   if (sent > 0) {
      _pendingOutput.remove(sent);
      return TRUE;
   } else {
      setRC(sent);
      return FALSE;
   }
}

SocketServer::SocketServer(unsigned int maxClients,
                           unsigned int maxWorkers,
                           const char* execPath,
                           unsigned int workerTimeout)
 : _maxClients(maxClients),
   _maxWorkers(maxWorkers),
   _execPath(execPath),
   _workerTimeout(workerTimeout),
   _nPollFDs(0) {
   _clients = new Client*[maxClients];
   for (unsigned int i = 0; i < maxClients; i++)
      _clients[i] = NULL;
   _workers = new Worker*[maxWorkers];
   for (unsigned int i = 0; i < maxWorkers; i++)
      _workers[i] = NULL;
   _pollFDs = new pollfd[1 + maxClients + maxWorkers];

   memset(_pollFDs, 0, sizeof(pollfd) * (1 + maxClients + maxWorkers));
}

int SocketServer::initSocket(unsigned int port) {
   int fd;

   if ((fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
      error("Can\'t create socket");
      return -1;
   }

// Re-use server addresses and set non-blocking

   if (!setReuseAddr(fd) || !setNonBlocking(fd)) {
      close(fd);
      return -1;
   }

// Populate the socket structure

   struct sockaddr_in addr;
   memset(&addr, 0, sizeof(addr));
   addr.sin_family = AF_INET;
   addr.sin_addr.s_addr = INADDR_ANY;
   addr.sin_port = htons(port);

   if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
      error("Can\'t bind to socket");
      close(fd);
      return -1;
   }

   if (listen(fd, DEFAULT_BACKLOG) == -1) {
      error("Can't listen on socket");
      close(fd);
      return -1;
   }

   return fd;
}

Client* SocketServer::findClient(int fd) {
   for (unsigned int i = 0; i < _maxClients; i++) {
      Client* client = _clients[i];
      if (client != NULL && fd == client->fd())
         return client;
   }
   return NULL;
}

Client* SocketServer::findClient(const Worker* worker) {
   for (unsigned int i = 0; i < _maxClients; i++) {
      Client* client = _clients[i];
      if (client != NULL &&
          client->recvEnable() &&
          worker == client->worker())
         return client;
   }
   return NULL;
}

Client* SocketServer::allocClient(int fd) {
   for (unsigned int i = 0; i < _maxClients; i++) {
      if (_clients[i] == NULL) {
         Client* client = new Client(fd);
         _clients[i] = client;
         return client;
      }
   }
   return NULL;
}

bool SocketServer::freeClient(Client* client) {
   for (unsigned int i = 0; i < _maxClients; i++) {
      if (_clients[i] == client) {
         delete client;
         _clients[i] = NULL;
         return TRUE;
      }
   }
   return FALSE;
}

Worker* SocketServer::findWorker(int fd) {
   for (unsigned int i = 0; i < _maxWorkers; i++) {
      Worker* worker = _workers[i];
      if (worker != NULL &&
          (fd == worker->pipeFD(1, 0) ||
           fd == worker->pipeFD(2, 0)))
         return worker;
   }
   return NULL;
}

Worker* SocketServer::findWorkerByPID(pid_t pid) {
   for (unsigned int i = 0; i < _maxWorkers; i++) {
      Worker* worker = _workers[i];
      if (worker != NULL && pid == worker->pid())
         return worker;
   }
   return NULL;
}

Worker* SocketServer::findSessionWorker(unsigned long sessionId) {
   for (unsigned int i = 0; i < _maxWorkers; i++) {
      Worker* worker = _workers[i];
      if (worker != NULL && sessionId == worker->sessionId())
         return worker;
   }
   return NULL;
}

Worker* SocketServer::checkInactivity(time_t time) {
   for (unsigned int i = 0; i < _maxWorkers; i++) {
      Worker* worker = _workers[i];
      if (worker != NULL && worker->sessionId() != 0 &&
          time - worker->lastAccessTime() >= _workerTimeout)
         return worker;
   }
   return NULL;
}

unsigned long SocketServer::genSessionId(void) {
   do {
      unsigned long sessionId = rand() % 100000000L;
      if (sessionId != 0) {
         bool match = FALSE;
         for (unsigned int i = 0; i < _maxWorkers; i++) {
            if (_workers[i] != NULL &&
               _workers[i]->sessionId() == sessionId) {
               match = TRUE;
               break;
            }
         }
         if (!match)
            return sessionId;
      }
   } while (TRUE);
}

Worker* SocketServer::allocWorker(unsigned long sessionId) {
   for (unsigned int i = 0; i < _maxWorkers; i++) {
      if (_workers[i] == NULL) {
         Worker* worker = new Worker(sessionId);
         _workers[i] = worker;
         return worker;
      }
   }
   return NULL;
}

void SocketServer::onSessionEnd(const Worker* worker) {
   for (unsigned int i = 0; i < _maxClients; i++) {
      Client* client = _clients[i];
      if (client != NULL &&
          worker == client->worker()) {
         const char* msg = !worker->timeout()
            ? ENDED
            : TIMEOUT;
         if (!client->send(msg, strlen(msg)))
            error("Failed to send to client");
      }
   }
}

bool SocketServer::freeWorker(Worker* worker) {
   for (unsigned int i = 0; i < _maxWorkers; i++) {
      if (_workers[i] == worker) {
         for (unsigned int j = 0; j < _maxClients; j++) {
            if (_clients[j] != NULL && _clients[j]->worker() == worker)
               _clients[j]->setWorker(NULL);
         }
         delete worker;
         _workers[i] = NULL;
         return TRUE;
      }
   }
   return FALSE;
}

bool SocketServer::init(unsigned int port) {
   int fd;

   if ((fd = initSocket(port)) < 0)
      return FALSE;

   _pollFDs[LISTEN_POLL_INDEX].fd = fd;
   _pollFDs[LISTEN_POLL_INDEX].events = POLLIN;
 
// Handle child signals with 'signalfd' file descriptor

   sigset_t mask;
   sigemptyset(&mask);
   sigaddset(&mask, SIGCHLD);
   sigprocmask(SIG_BLOCK, &mask, nullptr);

   int signalFD = signalfd(-1, &mask, 0);
   if (signalFD == -1) {
      error("Failed to create signalfd");
      return FALSE;
   }

   _pollFDs[SIGNAL_POLL_INDEX].fd = signalFD;
   _pollFDs[SIGNAL_POLL_INDEX].events = POLLIN;

// Inactivity timer

   int timerFD = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
   if (timerFD == -1) {
      error("Failed to create timer file descriptor");
      return FALSE;
   }

   struct itimerspec intervalTimerSpec;
   intervalTimerSpec.it_interval.tv_sec = TIMER_PERIOD;
   intervalTimerSpec.it_interval.tv_nsec = 0;
   intervalTimerSpec.it_value.tv_sec = TIMER_PERIOD;
   intervalTimerSpec.it_value.tv_nsec = 0;
   if (timerfd_settime(timerFD, 0, &intervalTimerSpec, nullptr) == -1) {
      error("Failed to set timer");
      return FALSE;
   }

   _pollFDs[TIMER_POLL_INDEX].fd = timerFD;
   _pollFDs[TIMER_POLL_INDEX].events = POLLIN;


   _nPollFDs = N_BASE_FDS;

   return TRUE;
}

int SocketServer::poll(void) {
   if (::poll(_pollFDs, _nPollFDs, -1) < 0) {
      error("poll() returned error");
      return 1;
   }

//   for (unsigned int i = 0; i < _nPollFDs; i++) {
   for (unsigned int i = _nPollFDs; i-- > 0;) {
      int fd = _pollFDs[i].fd;
      short revents = _pollFDs[i].revents;
      if (revents == 0)
         continue;

      if (i == LISTEN_POLL_INDEX) {
         if (revents != POLLIN) {
            fprintf(stderr, "Returned %d\n", revents);
            error("Unexpected poll event");
            return FALSE;
         }

         do {
            int clientFD = accept(fd, NULL, NULL);

//            printf("Accepted client fd: %d\n", clientFD);

            if (clientFD < 0) {
               if (errno != EWOULDBLOCK) {
                  error("accept() returned error");
                  return FALSE;
               }
               break;
            }

            if (clientFD >= 0) {
               if (allocClient(clientFD) == NULL) {
                  error("Failed to allocate a client");
                  break;
               }
                setNonBlocking(clientFD);
               _pollFDs[_nPollFDs].fd = clientFD;
               _pollFDs[_nPollFDs].events = POLLIN;
               _nPollFDs++;
//               printPollFDs();
               break;
            }

         } while (1);

      }
      else if (i == SIGNAL_POLL_INDEX) {
         if (revents != POLLIN) {
            fprintf(stderr, "Returned %d\n", revents);
            error("Unexpected poll event");
            return FALSE;
         }

         struct signalfd_siginfo sigInfo;
         ssize_t nRead = read(fd, &sigInfo, sizeof(sigInfo));
         if (nRead != sizeof(sigInfo)) {
            error("Failed to read signal");
            break;
         }

         if (sigInfo.ssi_signo == SIGCHLD) {
            pid_t childPID;
            while ((childPID = waitpid(-1, nullptr, WNOHANG)) > 0) {
               Worker* worker = findWorkerByPID(childPID);
               if (worker != NULL) {
                  if (worker->sessionId() == 0)
                     onSessionEnd(worker);
                  else {
                     Client* client = findClient(worker);
                     if (client != NULL) {
                        if (!client->send(ERROR_FAILED, strlen(ERROR_FAILED)))
                           error("Failed to send to client");

//--------------------- Client should respond by closing socket

                     }
                     else
                        error("No client for failed worker");
                  }

                  unsetPollFD(worker->pipeFD(1, 0));
                  unsetPollFD(worker->pipeFD(2, 0));
                  worker->closePipes();
                  freeWorker(worker);
               } else
                  error1("Received SIGCHLD from unknown process %d\n", childPID);
            }
         }      
      } else if (i == TIMER_POLL_INDEX) {
         if (!(revents & POLLIN)) {
            fprintf(stderr, "Unexpected timer event: %d\n", revents);
            continue;
         }

         uint64_t expiryCount;
         if (read(fd, &expiryCount, sizeof(expiryCount)) != sizeof(expiryCount))
            error("Failed to read timer");
 
         do {
            Worker* worker = checkInactivity(::time(NULL));
            if (worker == NULL)
               break;
            trace1("Worker PID %d inactivity timeout\n", worker->pid());
            worker->setTimeout();
            worker->endSession();
         } while (TRUE);

      } else {
         Client* client = findClient(fd);
         if (client != NULL) {
            client->send(NULL, 0); // send any pending output
            Worker* worker = client->worker();
            bool closeClient = FALSE;
            do {
               char recvBuffer[RECV_BUFFER_LEN];
               int len = ::recv(fd, recvBuffer, RECV_BUFFER_LEN, 0);
               if (len < 0) {
                  if (errno != EWOULDBLOCK) {
                     char buf[40];
                     sprintf(buf, "recv() failed, error %d\n", errno);
                     error(buf);
                     closeClient = TRUE;
                  }
                  break;
               }
               if (len == 0) {
//                  printf("Client connection closed\n");
                  closeClient = TRUE;
                  break;
               }
                   if (len >= 1)
                  client->pendingInput().add(recvBuffer, len);
            } while (TRUE);
 
            while (client->pendingInput().bufferLen() > 0) {
               if (worker == NULL || !worker->rcvdCmd()) {
                  char* buffer = client->pendingInput().buffer();
                  unsigned int bufferLen = client->pendingInput().bufferLen();

                  if (bufferLen > MAX_COMMAND_LEN) {
                     error("Command exceeds maximum length");
                     closeClient = TRUE;
                     break;
                  }

                  char* nlPtr = (char*)memchr(buffer, '\n', bufferLen);

                  if (nlPtr != NULL) {
                     if (worker == NULL) {
                        if (bufferLen > strlen(START_SESSION) &&
                           strncmp(buffer, START_SESSION, strlen(START_SESSION)) == 0) {

                           unsigned int sessionId = genSessionId();

                           trace1("Session id: %d\n", sessionId);
                           worker = allocWorker(sessionId);
                           if (worker == NULL) {
                              error("Failed to create worker");
                              closeClient = TRUE;
                              break;
                           }

                           if (!worker->createPipes()) {
                              error("Failed to create pipes");
                              freeWorker(worker);
                              closeClient = TRUE;
                              break;
                           }
                           pid_t parentPID = getpid();
                           pid_t pid = fork();
                           if (pid == 0) {
                              if (prctl(PR_SET_PDEATHSIG, SIGTERM) == -1) {
                                 perror("prctl failed");
                                 exit(1);
                              }

                              if (getppid() != parentPID)
                                 exit(1);

                              if (dup2(worker->pipeFD(0, 0), STDIN_FILENO) == -1 ||
                                  dup2(worker->pipeFD(1, 1), STDOUT_FILENO) == -1 ||
                                  dup2(worker->pipeFD(2, 1), STDERR_FILENO) == -1) {
                                 error("dup2 failed");
                                 ::exit(errno);
                              }

                              for (unsigned int i = 0; i < 3; i++)
                                 for (unsigned int j = 0; j < 2; j++)
                                    ::close(worker->pipeFD(i, j));

                              for (unsigned int j = 0; j < _nPollFDs; j++)
                                 ::close(_pollFDs[j].fd);

                              extern char** environ;
                              (void)::execve(_execPath, NULL, environ);

                              error("Failed to exec iss");
                              exit(1);
                           }

                           ::close(worker->pipeFD(0, 0));
                           ::close(worker->pipeFD(1, 1));
                           ::close(worker->pipeFD(2, 1));

                           worker->setPID(pid);
                           trace1("Created worker PID %d\n", pid);
                           worker->setRcvdCmd(TRUE);
                           _pollFDs[_nPollFDs].fd = worker->pipeFD(1, 0);
                           _pollFDs[_nPollFDs].events = POLLIN;
                           _nPollFDs++;
                           _pollFDs[_nPollFDs].fd = worker->pipeFD(2, 0);
                           _pollFDs[_nPollFDs].events = POLLIN;
                           _nPollFDs++;

                           ::send(fd, STARTED, strlen(STARTED), MSG_NOSIGNAL);
                           char sessionIdBuf[SESSION_ID_SIZE + 1];
                           sprintf(sessionIdBuf, "%08lu", sessionId);
                           ::send(fd, sessionIdBuf, strlen(sessionIdBuf), MSG_NOSIGNAL);
                           ::send(fd, NEWLINE, strlen(NEWLINE), MSG_NOSIGNAL);
                           client->setWorker(worker);
                        } else {
                           char* sepPtr = (char*)memchr(buffer, ':', bufferLen);
                           if (worker == NULL) {
                              if (sepPtr != NULL && sepPtr - buffer >= strlen(SESSION) &&
                                 strncmp(buffer, SESSION, strlen(SESSION)) == 0) {
                                  *nlPtr = '\0';
                                 worker = findSessionWorker(atol(sepPtr + 1));
                                 if (worker != NULL) {
                                    client->setWorker(worker);
                                    worker->setRcvdCmd(FALSE);
                                 } else {
                                    reportError(fd, UNKNOWN_SESSION_ID);
                                    break;
                                 }
                              } else {
                                 error("Invalid command");
                                 closeClient = TRUE;
                                 break;
                              }
                           }
                        }
                        client->pendingInput().remove(nlPtr + 1 - buffer);
                     } else {
                        if (bufferLen > strlen(END) &&
                           strncmp(buffer, END, strlen(END)) == 0) {
                           trace1("Killing process: %d\n", worker->pid());
                           worker->endSession();
                           client->pendingInput().remove(nlPtr + 1 - buffer);
                        }
                        else {
                           char* sepPtr = (char*)memchr(buffer, ':', bufferLen);
                           if (sepPtr != NULL && sepPtr - buffer >= strlen(SEND) &&
                              strncmp(buffer, SEND, strlen(SEND)) == 0) {
                              worker->setRcvdCmd(TRUE);
                              client->pendingInput().remove((sepPtr + 1) - buffer);
                              client->setAckSent();
                           } else if (sepPtr != NULL && sepPtr - buffer >= strlen(RECEIVE) &&
                              strncmp(buffer, RECEIVE, strlen(RECEIVE)) == 0) {
                              client->pendingInput().remove((nlPtr + 1) - buffer);
                              client->setRecvEnable(FALSE);
                           } else if (nlPtr != NULL && nlPtr - buffer >= strlen(STATUS) &&
                              strncmp(buffer, STATUS, strlen(STATUS)) == 0) {
                              client->pendingInput().remove((nlPtr + 1) - buffer);
                              client->send(READY, strlen(READY));
                           } else {
                              error("Invalid command");
                              closeClient = TRUE;
                              break;
                           }
                        }
                     }
                     if (!worker->rcvdCmd())
                        continue;
                  }
                  else
                     break;
               }

               int len = ::write(worker->pipeFD(0, 1),
                  client->pendingInput().buffer(),
                  client->pendingInput().bufferLen());

               if (len > 0)
                  client->pendingInput().remove(len);

               if (client->ackSent() &&
                   client->pendingInput().bufferLen() == 0 &&
                   !client->send(SENT, strlen(SENT)))
                  error("Failed to send to client");
            }

            if (closeClient) {
//               printf("Closing fd: %d\n", fd);
               close(fd);
               if (client != NULL)
                  freeClient(client);
               unsetPollFD(fd);
            } 
         } else {
            Worker* worker = findWorker(fd);
            if (worker != NULL) {
               Client* client = findClient(worker);
               if (client == NULL) {
                  error1("No client for worker, closing fd: %d\n", fd);
                  unsetPollFD(worker->pipeFD(1, 0));
                  unsetPollFD(worker->pipeFD(2, 0));
                  worker->closePipes();
                  freeWorker(worker);
               } else {
                  worker->setLastAccessTime(::time(NULL));
                  bool closeClient = FALSE;
                  do {
                     char readBuffer[READ_BUFFER_LEN];
                     int readLen = ::read(fd, readBuffer, READ_BUFFER_LEN);

                     if (readLen < 0) {
                        if (errno != EAGAIN) {
                           char buf[40];
                           sprintf(buf, "read() failed, error %d\n", errno);
                           error(buf);
                        }
                        break;
                     }
                     if (readLen == 0) {
                        unsetPollFD(worker->pipeFD(1, 0));
                        unsetPollFD(worker->pipeFD(2, 0));
                        break;
                     }
                     if (fd == worker->pipeFD(1, 0)) {
                        char buf[128];
                        sprintf(buf, "RECEIVED %d: ", readLen);
                        if (!client->send(buf, strlen(buf)) ||
                            !client->send(readBuffer, readLen))
                           error("Failed to send to client");
                     } else {
                        worker->stderrInput().add(readBuffer, readLen);
                        char* buffer = worker->stderrInput().buffer();
                        unsigned int bufferLen = worker->stderrInput().bufferLen();

                        if (bufferLen >= strlen(PROMPT) &&
                           strncmp(buffer, PROMPT, strlen(PROMPT)) == 0) {
                           if (!client->send(READY, strlen(READY)))
                              error("Failed to send to client");
                           worker->stderrInput().remove(bufferLen);
                        }
                     }
                  } while (TRUE);

                  if (closeClient) {
                     unsetPollFD(client->fd());
//                     printf("Closing client fd: %d\n", client->fd());
                     close(client->fd());
                     freeClient(client);
                  } else
                     client->send(NULL, 0);
               }

            }
            else
               error("Unknown file descriptor");
         }
      }
   }

   return TRUE;
}

void SocketServer::unsetPollFD(int fd) {
   for (unsigned int i = 0; i < _nPollFDs; i++) {
      if (_pollFDs[i].fd == fd) {

         if (i < _nPollFDs - 1) {
            _pollFDs[i] = _pollFDs[_nPollFDs - 1];
         }
         _nPollFDs--;
         _pollFDs[_nPollFDs].fd = -1;
         _pollFDs[_nPollFDs].events = 0;
         _pollFDs[_nPollFDs].revents = 0;
         return;
      }
   }
}

/*
void SocketServer::unsetPollFD(int fd) {
   for (unsigned int i = 0; i < _nPollFDs;) {
      if (_pollFDs[i].fd == fd) {
         for (unsigned int j = i; j < _nPollFDs; j++)
            _pollFDs[j].fd = _pollFDs[j + 1].fd;
         _nPollFDs--;
      } else
         i++;
   }
//   printPollFDs();
}
*/


void SocketServer::terminate(void) {
   ::close(_pollFDs[LISTEN_POLL_INDEX].fd);
   ::close(_pollFDs[SIGNAL_POLL_INDEX].fd);
}

void SocketServer::printPollFDs(void) {
   printf("Poll file descriptors: ");
   unsigned int n = 0;
   for (unsigned int i = 0; i < _nPollFDs; i++) {
      if (_pollFDs[i].fd != -1) {
         if (n != 0)
            printf(", ");
         printf("%d", _pollFDs[i].fd);
         n++;
      }
   }
   printf("\n");
}

void SocketServer::printBuffer(const PendingData& pendingData) {
   printf("Buffer: [");
   for (unsigned int i = 0; i < pendingData.bufferLen(); i++)
      printf("%c", pendingData.buffer()[i]);
   printf("]\n");
}

void respond(const char* buf, size_t len, Client& client) {
   ssize_t res;
   while (len > 0) {
      ssize_t res = send(client.fd(), buf, len, MSG_NOSIGNAL);
      if (res >= 0) {
         len -= res;
         buf += res;
      }
      else {
         client.setRC(res);
         return;
      }
   }
}
 
void closeSockets(void) {
/*
   int i;

   for (i = 0; i < socketState.nfd; i++)
      if (socketState.fds[i].fd >= 0)
         close(socketState.fds[i].fd);
*/
}
