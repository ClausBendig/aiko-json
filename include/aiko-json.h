/*
Copyright (c) 2010 Serge A. Zaitsev
Copyright (c) 2015 Alisdair McDiarmid
Copyright (c) 2015 Tony Wilk
Copyright (c) 2015 Claus Bendig

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#ifndef __JSON_H__
#define __JSON_H__

#define JSON_THREAD_SAFE

#ifdef JSON_THREAD_SAFE
	#include <pthread.h>
#endif

#ifndef JSON_TOKEN_SIZE
	#define JSON_TOKEN_SIZE (4096)
#endif

#ifndef JSON_JSON_SIZE
	#define JSON_JSON_SIZE (65536)
#endif

#ifndef JSON_STACK_DEPTH
	#define JSON_STACK_DEPTH (32)
#endif

#ifdef __cplusplus
extern "C" {
#endif

	// Enumerations.
	typedef enum {
		JSON_COMPACT = 0,
		JSON_PRETTY = 1
	} djson_format_e;

	typedef enum {
		JSON_PRIMITIVE = 0,
		JSON_OBJECT = 1,
		JSON_ARRAY = 2,
		JSON_STRING = 3
	} djson_type_e;

	typedef enum {
		/* Not enough tokens were provided. */
		JSON_ERROR_NOMEM = -1,
		/* Invalid character inside JSON string. */
		JSON_ERROR_INVAL = -2,
		/* The string is not a full JSON packet, more bytes expected. */
		JSON_ERROR_PART = -3,
		/* Output buffer is full. */
		JSON_ERROR_BUF_FULL = -4,
		/* Tried to write Array value into Object. */
		JSON_ERROR_NOT_ARRAY = -5,
		/* Tried to write Object key/value into Array. */
		JSON_ERROR_NOT_OBJECT = -6,
		/* Array/object nesting > JWR_STACK_DEPTH. */
		JSON_ERROR_STACK_FULL = -7,
		/* Stack underflow error (too many 'end's). */
		JSON_ERROR_STACK_EMPTY= -8,
		/* Nesting error, not all objects closed when jsonClose() called. */
		JSON_ERROR_NEST_ERROR = -9,
		/* Everything is ok. */
		JSON_ERROR_OK = 0
	} djson_error_e;


	struct struct_token_s {
		djson_type_e type;
		int start;
		int end;
		int size;
		int parent;
	};
	typedef struct struct_token_s token_s;

	typedef struct {
		djson_type_e nodeType;
		int elementNo;
	} nodestack_t;

	struct struct_parser_s {
		char *json;
		size_t counter;
		size_t length;
		token_s tokens[JSON_TOKEN_SIZE];
		#ifdef JSON_THREAD_SAFE
		pthread_mutex_t mutex;
		bool mutexInitialized;
		#endif
	};

	struct struct_unparser_s {
		char buffer[JSON_JSON_SIZE];						// pointer to application's buffer
		char *bufp;							// current write position in buffer
		char tmpbuf[32];					// local buffer for int/double convertions
		int error;							// error code
		int callNo;							// API call on which error occurred
		nodestack_t nodeStack[JSON_STACK_DEPTH];	// stack of array/object nodes
		int stackpos;
		int isPretty;						// 1= pretty output (inserts \n and spaces)
		#ifdef JSON_THREAD_SAFE
		pthread_mutex_t mutex;
		bool mutexInitialized;
		#endif
	};

	typedef struct struct_parser_s parser_s;
	typedef struct struct_unparser_s unparser_s;

	// Helpers.
	int getError(unparser_s *unparser);
	char *errorToString(int err);

	// Parsing.
	int startParsingJSON(parser_s *parser, char *js);
	int endParsingJSON(parser_s *parser);
	bool hasBeforeToken(parser_s *parser);
	bool hasCurrentToken(parser_s *parser);
	bool hasNextToken(parser_s *parser);
	bool tokenEqualString(parser_s *parser, char *s);
	char *tokenToString(parser_s *parser);
	int beforeTokenType(parser_s *parser);
	int currentTokenType(parser_s *parser);
	int nextTokenType(parser_s *parser);
	size_t sizeOfTokens(parser_s *parser);
	void beforeToken(parser_s *parser);
	void nextToken(parser_s *parser);

	// Unparsing
	int startUnparsingJSON(unparser_s *unparser, djson_type_e rootType, djson_format_e isPretty);
	int endUnparsingJSON(unparser_s *unparser);

	void addStringToObject(unparser_s *unparser, char *key, char *value);
	void addIntegerToObject(unparser_s *unparser, char *key, int value);
	void addDoubleToObject(unparser_s *unparser, char *key, double value);
	void addBooleanToObject(unparser_s *unparser, char *key, int oneOrZero);
	void addNullToObject(unparser_s *unparser, char *key);
	void addObjectToObject(unparser_s *unparser, char *key);
	void addArrayToObject(unparser_s *unparser, char *key);
	void addStringToArray(unparser_s *unparser, char *value);
	void addIntegerToArray(unparser_s *unparser, int value);
	void addDoubleToArray(unparser_s *unparser, double value);
	void addBooleanToArray(unparser_s *unparser, int oneOrZero);
	void addNullToArray(unparser_s *unparser);
	void addObjectToArray(unparser_s *unparser);
	void addArrayToArray(unparser_s *unparser);
	void addRawTextToObject(unparser_s *unparser, char *key, char *rawtext);
	void addRawTextToArray(unparser_s *unparser, char *rawtext);
	int endObject(unparser_s *unparser);
	int endArray(unparser_s *unparser);
	int endJSON(unparser_s *unparser);
	char *getJSON(unparser_s *unparser);
	size_t getJSONSize(unparser_s *unparser);

#ifdef __cplusplus
}
#endif

#endif
