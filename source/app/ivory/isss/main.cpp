/*------------------------------------------------------------------------------
 *
 * (c) Copyright 2000-2026 by Alasdair Scott
 *
 * Part of the Ivory System/Script project
 * 
 * https://ivorysystem.net | https://ivoryscript.net
 * https://github.com/IvorySystem | https://github.com/IvoryScript
 *
 * Name:    main.cpp
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
 *    A Linux process to implement a simple socket protocol to manage
 *    multiple worker processes.  Primarily designed for IvoryScript
 *    interaction.
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

#include <cstring>
#include <iostream>
#include <signal.h>
#include <unistd.h>
#include <linux/limits.h>

#include "OptionsParser.h"
#include "server.h"

#define DEFAULT_MAX_CLIENTS      16
#define DEFAULT_MAX_WORKERS      16
#define DEFAULT_SOCKET_PORT      4196
#define DEFAULT_EXEC             "/usr/local/bin/iss"

#define DEFAULT_WORKER_TIMEOUT   900   // 15 minutes
//#define DEFAULT_WORKER_TIMEOUT   30   // 30 seconds

using namespace std;

void error(const char* str) {
   std::cerr << str << '\n';
}
/*
void trace(const char* s) {
   time_t        _time;
   struct tm* _tm;
   char          _buf[128];

   time(&_time);

   _tm = localtime(&_time);

   strftime(_buf, 128, "%H:%M:%S %d/%m/%Y", _tm);
   std::cout << _buf << ": ";
   std::cout << s << '\n';
   std::cout.flush();
}
*/
void trace(const char* str) {
   std::cout << str << '\n';
}


bool getNumOption(const char* key, int argc, char* argv[], int& res) {
   for (int i = 1; i < argc; ++i) {
      OptionsParser optionsParser(argv[i]);
      if (optionsParser.getNum(key, res))
         return TRUE;
   }
   return FALSE;
}

bool getStringOption(const char* key, int argc, char* argv[], char* s) {
   for (int i = 1; i < argc; ++i) {
      OptionsParser optionsParser(argv[i]);
      const char* res = optionsParser.getString(key);
      if (res != NULL) {
         strcpy(s, res);
         return TRUE;
      }
   }
   return FALSE;
}



int main(int argc, char* argv[]) {
   trace("ISSS starting");

   signal(SIGPIPE, SIG_IGN);

   srand(time(NULL));

   int maxClients = DEFAULT_MAX_CLIENTS;
   (void)getNumOption("maxClients", argc, argv, maxClients);

   int maxWorkers = DEFAULT_MAX_WORKERS;
   (void)getNumOption("maxWorkers", argc, argv, maxWorkers);

   char execPath[PATH_MAX];
   if (!getStringOption("exec", argc, argv, execPath))
      strcpy(execPath, DEFAULT_EXEC);

   int workerTimeout = DEFAULT_WORKER_TIMEOUT;
   (void)getNumOption("workerTimeout", argc, argv, workerTimeout);

   SocketServer server(maxClients, maxWorkers, execPath, workerTimeout);

   int port = DEFAULT_SOCKET_PORT;
   (void)getNumOption("port", argc, argv, port);


   if (server.init((unsigned int)port)) {
      do {
         (void)server.poll();
      } while (true);

      server.terminate();
   }

   trace("ISSS ending");

   return 0;

}
