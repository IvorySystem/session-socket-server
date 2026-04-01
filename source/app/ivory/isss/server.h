/*------------------------------------------------------------------------------
 *
 * (c) Copyright 2000-2026 by Alasdair Scott
 *
 * Part of the Ivory System/Script project
 *
 * https://ivorysystem.net | https://ivoryscript.net
 * https://github.com/IvorySystem | https://github.com/IvoryScript
 *
 * Name:    server.h
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
 * Definitions for use with a session socket server.
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


#define FALSE 0
#define TRUE  1

#define SESSION_ID_SIZE 8

#define GEN_BUF_SIZE 2048

#define error1(fmt,arg)\
{char errBuf[GEN_BUF_SIZE];\
sprintf(errBuf,(fmt),(arg));\
::error(errBuf);}

 // Pending data for a socket

class PendingData {
public:
   PendingData(void);
   ~PendingData() { delete[] _buffer; }
   char* buffer(void) { return _buffer; }
   const char* buffer(void) const { return _buffer; }
   unsigned int bufferLen(void) const { return _bufferLen; }

   void add(const char* data, unsigned int len);
   void remove(unsigned int len);

protected:
   char* _buffer;
   int   _bufferSize;
   int   _bufferLen;
};

class Client;

// A worker process

class Worker {
public:
   Worker(unsigned long sessionId);

   unsigned long sessionId(void) const { return _sessionId; }
   pid_t pid(void) const { return _pid; }
   int pipeFD(unsigned int index1, unsigned int index2) const { return _pipeFD[index1][index2]; }
   time_t lastAccessTime(void) const { return _lastAccessTime; }

   bool rcvdCmd(void) const { return _rcvdCmd; }
   bool timeout(void) const { return _timeout; }
   PendingData& stderrInput(void) { return _stderrInput; }

   void setPID(pid_t pid) { _pid = pid; }
   void setRcvdCmd(bool q) { _rcvdCmd = q; }
   void setLastAccessTime(time_t time) { _lastAccessTime = time; }
   void setTimeout(void) { _timeout = TRUE; }

   bool createPipes(void);

   void endSession(void);

   void closePipes(void);

protected:
   unsigned long  _sessionId;
   pid_t          _pid;
   int            _pipeFD[3][2];
   bool           _rcvdCmd;
   time_t         _lastAccessTime;
   bool           _timeout;
   PendingData    _stderrInput;
};

// A client socket (typically a web server script)

class Client {
public:
   Client(int fd);
   int fd(void) { return _fd; }
   int rc(void) { return _rc; }
   bool ackSent(void) { return _ackSent; }
   bool recvEnable(void) { return _recvEnable; }
   void setRC(int rc) { _rc = rc; }
   void setAckSent(void) { _ackSent = true; }
   void setRecvEnable(bool recvEnable) { _recvEnable = recvEnable; }
   void setWorker(Worker* worker) { _worker = worker; }
   Worker* worker(void) { return _worker; }
   PendingData& pendingInput(void)  { return _pendingInput; }
   PendingData& pendingOutput(void) { return _pendingOutput; }

   bool send(const char* buf, unsigned int len);

protected:
   int            _fd;
   int            _rc;
   bool           _ackSent;
   bool           _recvEnable;
   Worker*        _worker;
   PendingData    _pendingInput;
   PendingData    _pendingOutput;
};

// Socket server

class SocketServer {
public:
   SocketServer(unsigned int maxClients, unsigned int maxWorkers,
                const char* execPath, unsigned int workerTimeout);
      
   bool init(unsigned);
   int poll(void);
   void terminate(void);

protected:
   int initSocket(unsigned int port);
   Client* findClient(int fd);
   Client* findClient(const Worker* worker);
   Client* allocClient(int fd);
   bool freeClient(Client* client);
   Worker* findSessionWorker(unsigned long sessionId);
   Worker* findWorker(int fd);
   Worker* findWorkerByPID(pid_t pid);
   unsigned long genSessionId(void);
   Worker* allocWorker(unsigned long sessionId);
   void onSessionEnd(const Worker* worker);
   Worker* checkInactivity(time_t time);
   bool freeWorker(Worker* worker);
   void unsetPollFD(int fd);

   void printPollFDs(void);
   void printBuffer(const PendingData& pendingData);

   unsigned int   _maxClients;
   unsigned int   _maxWorkers;

   const char*    _execPath;

   unsigned int   _workerTimeout;

   unsigned int   _nPollFDs;
   struct pollfd* _pollFDs;

   Client**       _clients;

   Worker**       _workers;
};

