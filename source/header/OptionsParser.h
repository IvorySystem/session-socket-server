/*------------------------------------------------------------------------------
 *
 * (c) Copyright 2000-2026 by Alasdair Scott
 *
 * Part of the Ivory System/Script project
 *
 * https://ivorysystem.net | https://ivoryscript.net
 * https://github.com/IvorySystem | https://github.com/IvoryScript
 *
 * Name:    OptionsParser.h
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

#ifndef OPTIONS_PARSER_H_DEFINED
#define OPTIONS_PARSER_H_DEFINED

#include <stdio.h>

#define MAX_OPTIONS_KEY_LENGTH 256

class OptionsParser {
protected:
   enum Token {
      TOKEN_NULL,
      TOKEN_ERR,
      TOKEN_KEY,
      TOKEN_MINUS_SIGN,
      TOKEN_NUMBER,
      TOKEN_STRING,
      TOKEN_EQ,
      TOKEN_END
   };
public:
   OptionsParser (const char* input);

   bool hasKey(const char* key);
   const char* getString(const char* key);
   bool getNum(const char* key, int& res);

protected:
   void getCh(void);
   void getNumber(void);
   void nextToken(void);
   void getKey(void);

   bool option(const char* key);
   bool optionSequence(const char* key);

   bool parse(const char* key);

protected:
   const char*    _input;
   Token          _token;
   char           _c;
   int            _number;
   char           _word[MAX_OPTIONS_KEY_LENGTH];
};

#endif /* OPTIONS_PARSER_H_DEFINED */
