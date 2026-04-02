/*------------------------------------------------------------------------------
 *
 * (c) Copyright 2000-2026 by Alasdair Scott
 *
 * Part of the Ivory System/Script project
 *
 * https://ivorysystem.net | https://ivoryscript.net
 * https://github.com/IvorySystem | https://github.com/IvoryScript
 *
 * Name:    OptionsParser.cpp
 *
 * Module:  General
 *
 * Author:  Alasdair Scott
 *
 * Original date: 2 February 2000
 *
 *------------------------------------------------------------------------------
 *
 * Description:
 *
 *    A class to parse command line options.
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

#include <ctype.h>
#include <string.h>
#include "general.h"
#include "OptionsParser.h"

#ifdef __GNUC__
#define _strcmpi strcasecmp
#endif

OptionsParser::OptionsParser(const char* input)
   : _input(input), _token(TOKEN_NULL) {
}

void OptionsParser::getCh(void) {
   _c = *_input;
   if (_c != '\0')
      _input++;
}

void OptionsParser::getKey(void) {
   char* cp = _word;
   do {
      *cp++ = _c;
      getCh();
      if (!isalpha(_c) && _c != '_')
         break;
   } while (true);
   *cp++ = '\0';
}

void OptionsParser::getNumber(void) {
   _number = 0;
   do {
      _number = _number * 10 + (_c - '0');
      getCh();
   } while (isdigit(_c));
}

void OptionsParser::nextToken(void) {
   while (_c != '\0') {
      if (isalpha(_c)) {
         _token = TOKEN_KEY;
         getKey();
         return;
      } else if (isdigit(_c)) {
         _token = TOKEN_NUMBER;
         getNumber();
         return;
      } else {
         switch (_c) {
            case ' ':
            case '\t':
            case '\n':
               do {
                  getCh();
               } while (isspace(_c));
               continue;

            case '-':
               _token = TOKEN_MINUS_SIGN;
               break;

            case '=':
               _token = TOKEN_EQ;
               break;
          
            case '"': {
                  char* cp = _word;
                  unsigned int n = 0;
                  do {
                     getCh();
                     if (_c == '\0') {
                        _token = TOKEN_ERR;
                        return;
                     }
                     if (_c == '"')
                        break;
                     if (n < MAX_OPTIONS_KEY_LENGTH - 1) {
                        *cp++ = _c;
                        n++;
                     }
                  } while (true);
                  *cp = '\0';
               }
               _token = TOKEN_STRING;
               break;

            default:
               break;
         }
         getCh();
         return;
      }
   }
   _token = TOKEN_END;
}
/*
bool OptionsParser::option(const char* key) {
   if (_token == TOKEN_KEY) {
      bool match = _strcmpi(_word, key) == 0;
      nextToken();
      if (_token == TOKEN_EQ) {
         nextToken();
         if (_token == TOKEN_MINUS_SIGN) {
            nextToken();
            _number = -_number;
         }
         if (!match)
            nextToken();
      }
      return match;
   } else
      return false;
}
*/
bool OptionsParser::option(const char* key) {
   if (_token != TOKEN_KEY)
      return false;

   bool match = (_strcmpi(_word, key) == 0);
   nextToken();

   if (_token == TOKEN_EQ) {
      nextToken();
      if (_token == TOKEN_MINUS_SIGN) {
         nextToken();
         _number = -_number;
      }
      if (!match)
         nextToken();
   }
   return match;
}


bool OptionsParser::optionSequence(const char* key) {
   while (_token != TOKEN_END) {
      if (option(key))
         return true;
   }
   return false;
}

bool OptionsParser::parse(const char* key) {
   Bool res;
   const char * input = _input;
   _token = TOKEN_NULL;
   if (_input != NULL) {
      getCh();
      nextToken();
      res = optionSequence(key);
   } else
      res = false;
   _input = input;
   return res;
}

// hasKey: Return true if key present, false otherwise

bool OptionsParser::hasKey(const char* key) {
   return parse(key);
}

// Return string option for a given key

const char* OptionsParser::getString(const char* key) {
   return parse(key) ? _word
                     : NULL;
}

// Return numeric option for a given key

bool OptionsParser::getNum(const char* key, int& res) {
   if (parse(key)) {
      res = _number;
      return true;
   } else
      return false;
}
