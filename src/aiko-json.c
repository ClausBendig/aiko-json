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

#include <stddef.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include "aiko-json.h"

/**
 * JSON parser. Contains an array of token blocks available. Also stores
 * the string being parsed now and current position in that string
 */
typedef struct {
	unsigned int pos; /* offset in the JSON string */
	unsigned int toknext; /* next token to allocate */
	int toksuper; /* superior token node, e.g parent object or array */
} jsmne_parser;

/**
 * Allocates a fresh unused token from the token pull.
 */
static token_s *jsmne_alloc_token(jsmne_parser *parser,
		token_s *tokens, size_t num_tokens) {
	token_s *tok;
	if (parser->toknext >= num_tokens) {
		return NULL;
	}
	tok = &tokens[parser->toknext++];
	tok->start = tok->end = -1;
	tok->size = 0;
	tok->parent = -1;
	return tok;
}

/**
 * Fills token type and boundaries.
 */
static void jsmne_fill_token(token_s *token, djson_type_e type,
                            int start, int end) {
	token->type = type;
	token->start = start;
	token->end = end;
	token->size = 0;
}

/**
 * Fills next available token with JSON primitive.
 */
static djson_error_e jsmne_parse_primitive(jsmne_parser *parser, const char *js,
		size_t len, token_s *tokens, size_t num_tokens) {
	token_s *token;
	int start;

	start = parser->pos;

	for (; parser->pos < len && js[parser->pos] != '\0'; parser->pos++) {
		switch (js[parser->pos]) {
			case '\t' : case '\r' : case '\n' : case ' ' :
			case ','  : case ']'  : case '}' :
				goto found;
		}
		if (js[parser->pos] < 32 || js[parser->pos] >= 127) {
			parser->pos = start;
			return JSON_ERROR_INVAL;
		}
	}

found:
	if (tokens == NULL) {
		parser->pos--;
		return 0;
	}
	token = jsmne_alloc_token(parser, tokens, num_tokens);
	if (token == NULL) {
		parser->pos = start;
		return JSON_ERROR_NOMEM;
	}
	jsmne_fill_token(token, JSON_PRIMITIVE, start, parser->pos);
	token->parent = parser->toksuper;
	parser->pos--;
	return 0;
}

/**
 * Filsl next token with JSON string.
 */
static djson_error_e jsmne_parse_string(jsmne_parser *parser, const char *js,
		size_t len, token_s *tokens, size_t num_tokens) {
	token_s *token;

	int start = parser->pos;

	parser->pos++;

	/* Skip starting quote */
	for (; parser->pos < len && js[parser->pos] != '\0'; parser->pos++) {
		char c = js[parser->pos];

		/* Quote: end of string */
		if (c == '\"') {
			if (tokens == NULL) {
				return 0;
			}
			token = jsmne_alloc_token(parser, tokens, num_tokens);
			if (token == NULL) {
				parser->pos = start;
				return JSON_ERROR_NOMEM;
			}
			jsmne_fill_token(token, JSON_STRING, start+1, parser->pos);
			token->parent = parser->toksuper;
			return 0;
		}

		/* Backslash: Quoted symbol expected */
		if (c == '\\' && parser->pos + 1 < len) {
			int i;
			parser->pos++;
			switch (js[parser->pos]) {
				/* Allowed escaped symbols */
				case '\"': case '/' : case '\\' : case 'b' :
				case 'f' : case 'r' : case 'n'  : case 't' :
					break;
				/* Allows escaped symbol \uXXXX */
				case 'u':
					parser->pos++;
					for(i = 0; i < 4 && parser->pos < len && js[parser->pos] != '\0'; i++) {
						/* If it isn't a hex character we have an error */
						if(!((js[parser->pos] >= 48 && js[parser->pos] <= 57) || /* 0-9 */
									(js[parser->pos] >= 65 && js[parser->pos] <= 70) || /* A-F */
									(js[parser->pos] >= 97 && js[parser->pos] <= 102))) { /* a-f */
							parser->pos = start;
							return JSON_ERROR_INVAL;
						}
						parser->pos++;
					}
					parser->pos--;
					break;
				/* Unexpected symbol */
				default:
					parser->pos = start;
					return JSON_ERROR_INVAL;
			}
		}
	}
	parser->pos = start;
	return JSON_ERROR_PART;
}

/**
 * Parse JSON string and fill tokens.
 */
static djson_error_e jsmne_parse(jsmne_parser *parser, const char *js, size_t len,
		token_s *tokens, unsigned int num_tokens) {
	djson_error_e r;
	int i;
	token_s *token;
	int count = 0;
    int k = 0;

	for (; parser->pos < len && js[parser->pos] != '\0'; parser->pos++) {
		char c;
		djson_type_e type;

		c = js[parser->pos];
		switch (c) {
			case '{': case '[':
				count++;
				if (tokens == NULL) {
					break;
				}
				token = jsmne_alloc_token(parser, tokens, num_tokens); k++;
				if (token == NULL)
					return JSON_ERROR_NOMEM;
				if (parser->toksuper != -1) {
					tokens[parser->toksuper].size++;
					token->parent = parser->toksuper;
				}
				token->type = (c == '{' ? JSON_OBJECT : JSON_ARRAY);
				token->start = parser->pos;
				parser->toksuper = parser->toknext - 1;
				break;
			case '}': case ']':
				if (tokens == NULL)
					break;
				type = (c == '}' ? JSON_OBJECT : JSON_ARRAY);
				if (parser->toknext < 1) {
					return JSON_ERROR_INVAL;
				}
				token = &tokens[parser->toknext - 1];
				for (;;) {
					if (token->start != -1 && token->end == -1) {
						if (token->type != type) {
							return JSON_ERROR_INVAL;
						}
						token->end = parser->pos + 1;
						parser->toksuper = token->parent;
						break;
					}
					if (token->parent == -1) {
						break;
					}
					token = &tokens[token->parent];
				}
				break;
			case '\"':
				r = jsmne_parse_string(parser, js, len, tokens, num_tokens); k++;
				if (r < 0) return r;
				count++;
				if (parser->toksuper != -1 && tokens != NULL)
					tokens[parser->toksuper].size++;
				break;
			case '\t' : case '\r' : case '\n' : case ' ':
				break;
			case ':':
				parser->toksuper = parser->toknext - 1;
				break;
			case ',':
				if (tokens != NULL &&
						tokens[parser->toksuper].type != JSON_ARRAY &&
						tokens[parser->toksuper].type != JSON_OBJECT) {
					parser->toksuper = tokens[parser->toksuper].parent;
					for (i = parser->toknext - 1; i >= 0; i--) {
						if (tokens[i].type == JSON_ARRAY || tokens[i].type == JSON_OBJECT) {
							if (tokens[i].start != -1 && tokens[i].end == -1) {
								parser->toksuper = i;
								break;
							}
						}
					}
				}
				break;

			/* In strict mode primitives are: numbers and booleans */
			case '-': case '0': case '1' : case '2': case '3' : case '4':
			case '5': case '6': case '7' : case '8': case '9':
			case 't': case 'f': case 'n' :
				/* And they must not be keys of the object */
				if (tokens != NULL) {
					token_s *t = &tokens[parser->toksuper];
					if (t->type == JSON_OBJECT ||
							(t->type == JSON_STRING && t->size != 0)) {
						return JSON_ERROR_INVAL;
					}
				}
				r = jsmne_parse_primitive(parser, js, len, tokens, num_tokens); k++;
				if (r < 0) {
                    return r;
                }
				count++;
				if (parser->toksuper != -1 && tokens != NULL)
					tokens[parser->toksuper].size++;
				break;

			/* Unexpected char in strict mode */
			default:
				return JSON_ERROR_INVAL;
		}
	}

	for (i = parser->toknext - 1; i >= 0; i--) {
		/* Unmatched opened object or array */
		if (tokens[i].start != -1 && tokens[i].end == -1) {
			return JSON_ERROR_PART;
		}
	}
	return k;
}

/**
 * Creates a new parser based over a given  buffer with an array of tokens
 * available.
 */
static void jsmne_init(jsmne_parser *parser) {
	parser->pos = 0;
	parser->toknext = 0;
	parser->toksuper = -1;
}

static void strreverse(char* begin, char* end) {
    char aux;
    while (end > begin)
        aux = *end, *end-- = *begin, *begin++ = aux;
}

static void modp_itoa10(int32_t value, char* str) {
    char* wstr=str;
    // Take care of sign
    unsigned int uvalue = (value < 0) ? -value : value;
    // Conversion. Number is reversed.
    do *wstr++ = (char)(48 + (uvalue % 10)); while(uvalue /= 10);
    if (value < 0) *wstr++ = '-';
    *wstr='\0';

    // Reverse string
    strreverse(str,wstr-1);
}

static const double pow10[] = { 1, 10, 100, 1000, 10000, 100000, 1000000, 10000000, 100000000, 1000000000 };
static void modp_dtoa2(double value, char* str, int prec) {
    /* if input is larger than thres_max, revert to exponential */
    const double thres_max = (double)(0x7FFFFFFF);
    int count;
    double diff = 0.0;
    char* wstr = str;
	int neg= 0;
	int whole;
	double tmp;
	uint32_t frac;

    /* Hacky test for NaN
     * under -fast-math this won't work, but then you also won't
     * have correct nan values anyways.  The alternative is
     * to link with libmath (bad) or hack IEEE double bits (bad)
     */
    if (! (value == value)) {
        str[0] = 'n'; str[1] = 'a'; str[2] = 'n'; str[3] = '\0';
        return;
    }

    if (prec < 0) {
        prec = 0;
    } else if (prec > 9) {
        /* precision of >= 10 can lead to overflow errors */
        prec = 9;
    }

    /* we'll work in positive values and deal with the
       negative sign issue later */
    if (value < 0) {
        neg = 1;
        value = -value;
    }


    whole = (int) value;
    tmp = (value - whole) * pow10[prec];
    frac = (uint32_t)(tmp);
    diff = tmp - frac;

    if (diff > 0.5) {
        ++frac;
        /* handle rollover, e.g.  case 0.99 with prec 1 is 1.0  */
        if (frac >= pow10[prec]) {
            frac = 0;
            ++whole;
        }
    } else if (diff == 0.5 && ((frac == 0) || (frac & 1))) {
        /* if halfway, round up if odd, OR
           if last digit is 0.  That last part is strange */
        ++frac;
    }

    /* for very large numbers switch back to native sprintf for exponentials.
       anyone want to write code to replace this? */
    /*
      normal printf behavior is to print EVERY whole number digit
      which can be 100s of characters overflowing your buffers == bad
    */
    if (value > thres_max) {
        sprintf(str, "%e", neg ? -value : value);
        return;
    }

    if (prec == 0) {
        diff = value - whole;
        if (diff > 0.5) {
            /* greater than 0.5, round up, e.g. 1.6 -> 2 */
            ++whole;
        } else if (diff == 0.5 && (whole & 1)) {
            /* exactly 0.5 and ODD, then round up */
            /* 1.5 -> 2, but 2.5 -> 2 */
            ++whole;
        }

        //vvvvvvvvvvvvvvvvvvv  Diff from modp_dto2
    } else if (frac) {
        count = prec;
        // now do fractional part, as an unsigned number
        // we know it is not 0 but we can have leading zeros, these
        // should be removed
        while (!(frac % 10)) {
            --count;
            frac /= 10;
        }
        //^^^^^^^^^^^^^^^^^^^  Diff from modp_dto2

        // now do fractional part, as an unsigned number
        do {
            --count;
            *wstr++ = (char)(48 + (frac % 10));
        } while (frac /= 10);
        // add extra 0s
        while (count-- > 0) *wstr++ = '0';
        // add decimal
        *wstr++ = '.';
    }

    // do whole part
    // Take care of sign
    // Conversion. Number is reversed.
    do *wstr++ = (char)(48 + (whole % 10)); while (whole /= 10);
    if (neg) {
        *wstr++ = '-';
    }
    *wstr='\0';
    strreverse(str, wstr-1);
}

static void jwPutch(unparser_s *unparser, char c) {
	if( (unsigned int)(unparser->bufp - unparser->buffer) >= JSON_JSON_SIZE ) {
		unparser->error= JSON_ERROR_BUF_FULL;
	} else {
		*unparser->bufp++ = c;
	}
}

static void jwPutstr(unparser_s *unparser, char *str) {
	jwPutch( unparser, '\"' );
	while( *str != '\0' ) jwPutch( unparser, *str++ );
	jwPutch( unparser, '\"' );
}

static void jwPutraw(unparser_s *unparser, char *str) {
	while( *str != '\0' ) jwPutch( unparser, *str++ );
}

static void jwPretty(unparser_s *unparser) {
	int i;
	if( unparser->isPretty ) {
		jwPutch( unparser, '\n' );
		for( i=0; i < unparser->stackpos + 1; i++ )
			jwPutraw( unparser, "    " );
	}
}

static void jwPush(unparser_s *unparser, djson_type_e nodeType) {
	if( (unparser->stackpos + 1) >= JSON_STACK_DEPTH )
		unparser->error = JSON_ERROR_STACK_FULL;		// array/object nesting > JSON_STACK_DEPTH
	else {
		unparser->nodeStack[++unparser->stackpos].nodeType= nodeType;
		unparser->nodeStack[unparser->stackpos].elementNo= 0;
	}
}

static djson_type_e jwPop(unparser_s *unparser) {
	djson_type_e retval= unparser->nodeStack[unparser->stackpos].nodeType;
	if( unparser->stackpos == 0 )
		unparser->error = JSON_ERROR_STACK_EMPTY;		// stack underflow error (too many 'end's)
	else
		unparser->stackpos--;
	return retval;
}

static int _jwObj(unparser_s *unparser, char *key) {
	if(unparser->error == JSON_ERROR_OK) {
		unparser->callNo++;
		if(unparser->nodeStack[unparser->stackpos].nodeType != JSON_OBJECT)
			unparser->error = JSON_ERROR_NOT_OBJECT;			// tried to write Object key/value into Array
		else if( unparser->nodeStack[unparser->stackpos].elementNo++ > 0 )
			jwPutch( unparser, ',' );
		jwPretty( unparser );
		jwPutstr( unparser, key );
		jwPutch( unparser, ':' );
		if(unparser->isPretty)
			jwPutch( unparser, ' ' );
	}
	return unparser->error;
}

static int _jwArr(unparser_s *unparser) {
	if(unparser->error == JSON_ERROR_OK) {
		unparser->callNo++;
		if( unparser->nodeStack[unparser->stackpos].nodeType != JSON_ARRAY )
			unparser->error= JSON_ERROR_NOT_ARRAY;			// tried to write array value into Object
		else if( unparser->nodeStack[unparser->stackpos].elementNo++ > 0 )
			jwPutch( unparser, ',' );
		jwPretty( unparser );
	}
	return unparser->error;
}

/**
 * Parse JSON by overgiven it and his length.
 * It returns a buffer with an array of tokens.
 */
int startParsingJSON(parser_s *parser, char *js) {
	#ifdef JSON_THREAD_SAFE
	if (parser->mutexInitialized == false) {
		pthread_mutex_init (&parser->mutex, NULL);
		parser->mutexInitialized = true;
	}
	pthread_mutex_lock (&parser->mutex);
	#endif

    jsmne_parser p;
    jsmne_init(&p);

    int len = jsmne_parse(&p, js, strlen(js), &parser->tokens[0], JSON_TOKEN_SIZE);

    while (len == JSON_ERROR_NOMEM) {
       return JSON_ERROR_NOMEM;
    }

	parser->counter = 0;
	parser->length = len;
	parser->json = js;

    return JSON_ERROR_OK;
}

/*
 * Free tokens.
 */
int endParsingJSON(parser_s *parser) {
	#ifdef JSON_THREAD_SAFE
	pthread_mutex_unlock (&parser->mutex);
	#endif
	return JSON_ERROR_OK;
}

token_s *getBeforeToken(parser_s *parser) {
	if ((parser->counter > 0) && (parser->counter <= (parser->length - 1))) {
		return &parser->tokens[parser->counter - 1];
	} else {
		return NULL;
	}
}

token_s *getCurrentToken(parser_s *parser) {
	if ((parser->counter >= 0) && (parser->counter <= (parser->length - 1))) {
		return &parser->tokens[parser->counter];
	} else {
		return NULL;
	}
}

token_s *getNextToken(parser_s *parser) {
	if ((parser->counter >= 0) && (parser->counter < (parser->length - 1))) {
		return &parser->tokens[parser->counter + 1];
	} else {
		return NULL;
	}
}

bool hasBeforeToken(parser_s *parser) {
	if (parser->counter > 0)  {
		return true;
	} else {
		return false;
	}
}

bool hasCurrentToken(parser_s *parser) {
	if ((parser->counter >= 0) && (parser->counter <= (parser->length - 1))) {
		return true;
	} else {
		return false;
	}
}

bool hasNextToken(parser_s *parser) {
	if (parser->counter < (parser->length - 1))  {
		return true;
	} else {
		return false;
	}
}

void beforeToken(parser_s *parser) {
	parser->counter = parser->counter - 1;
}

void nextToken(parser_s *parser) {
	parser->counter = parser->counter + 1;
}

size_t sizeOfTokens(parser_s *parser) {
	return parser->length;
}

/*
 * Check if token is equal to a string.
 */
bool tokenEqualString(parser_s *parser, char *s) {
    return (strncmp(parser->json + parser->tokens[parser->counter].start, s, parser->tokens[parser->counter].end - parser->tokens[parser->counter].start) == 0
            && strlen(s) == (size_t) (parser->tokens[parser->counter].end - parser->tokens[parser->counter].start));
}

/*
 * Return the string of the token.
 */
char* tokenToString(parser_s *parser) {
    parser->json[parser->tokens[parser->counter].end] = '\0';
    return parser->json + parser->tokens[parser->counter].start;
}

int beforeTokenType(parser_s *parser) {
	if (parser->counter > 0)  {
		return parser->tokens[parser->counter - 1].type;
	} else {
		return -1;
	}
}

int currentTokenType(parser_s *parser) {
	if ((parser->counter >= 0) && (parser->counter <= (parser->length - 1))) {
		return parser->tokens[parser->counter].type;
	} else {
		return -1;
	}
}

int nextTokenType(parser_s *parser) {
	if (parser->counter < (parser->length - 1))  {
		return parser->tokens[parser->counter + 1].type;
	} else {
		return -1;
	}
}

int startUnparsingJSON(unparser_s *unparser, djson_type_e rootType, djson_format_e isPretty) {
	#ifdef JSON_THREAD_SAFE
	if (unparser->mutexInitialized == false) {
		pthread_mutex_init (&unparser->mutex, NULL);
		unparser->mutexInitialized = true;
	}
	pthread_mutex_lock (&unparser->mutex);
	#endif

	unparser->bufp= &unparser->buffer[0];
	unparser->nodeStack[0].nodeType= rootType;
	unparser->nodeStack[0].elementNo= 0;
	unparser->stackpos = 0;
	unparser->error = JSON_ERROR_OK;
	unparser->callNo = 1;
	unparser->isPretty= isPretty;
	jwPutch( unparser, (rootType==JSON_STRING) ? '{' : '[' );
	return JSON_ERROR_OK;
}

int endUnparsingJSON(unparser_s *unparser) {
	#ifdef JSON_THREAD_SAFE
	pthread_mutex_unlock (&unparser->mutex);
	#endif
	return JSON_ERROR_OK;
}

char *getJSON(unparser_s *unparser) {
	return &unparser->buffer[0];
}

size_t getJSONSize(unparser_s *unparser) {
	return strlen(&unparser->buffer[0]);
}

int endJSON(unparser_s *unparser) {
	if( unparser->error == JSON_ERROR_OK ) {
		if( unparser->stackpos == 0 ) {
			djson_type_e node= unparser->nodeStack[0].nodeType;
			if(unparser->isPretty) jwPutch( unparser, '\n' );
			jwPutch( unparser, (node == JSON_OBJECT) ? '}' : ']');
		} else {
			unparser->error = JSON_ERROR_NEST_ERROR;	// nesting error, not all objects closed when unparserlose() called
		}
	}
	return unparser->error;
}

int endObject(unparser_s *unparser) {
	if( unparser->error == JSON_ERROR_OK ) {
		djson_type_e node;
		int lastElemNo= unparser->nodeStack[unparser->stackpos].elementNo;
		node= jwPop( unparser );
		if( lastElemNo > 0 ) jwPretty( unparser );
		jwPutch( unparser, (node == JSON_OBJECT) ? '}' : ']');
	}
	return unparser->error;
}

int endArray(unparser_s *unparser) {
	if( unparser->error == JSON_ERROR_OK ) {
		djson_type_e node;
		int lastElemNo= unparser->nodeStack[unparser->stackpos].elementNo;
		node= jwPop( unparser );
		if( lastElemNo > 0 ) jwPretty( unparser );
		jwPutch( unparser, (node == JSON_OBJECT) ? '}' : ']');
	}
	return unparser->error;
}

int getError(unparser_s *unparser) {
	return unparser->callNo;
}

void addRawTextToObject(unparser_s *unparser, char *key, char *rawtext) {
	if(_jwObj( unparser, key ) == JSON_ERROR_OK) jwPutraw( unparser, rawtext);
}

void addStringToObject(unparser_s *unparser, char *key, char *value) {
	if(_jwObj( unparser, key ) == JSON_ERROR_OK) jwPutstr( unparser, value );
}

void addIntegerToObject(unparser_s *unparser, char *key, int value) {
	modp_itoa10( value, unparser->tmpbuf );
	addRawTextToObject( unparser, key, unparser->tmpbuf );
}

void addDoubleToObject(unparser_s *unparser, char *key, double value) {
	modp_dtoa2( value, unparser->tmpbuf, 6 );
	addRawTextToObject( unparser, key, unparser->tmpbuf );
}

void addBooleanToObject(unparser_s *unparser, char *key, int oneOrZero) {
	addRawTextToObject( unparser, key, (oneOrZero) ? "true" : "false" );
}

void addNullToObject(unparser_s *unparser, char *key) {
	addRawTextToObject( unparser, key, "null" );
}

void addObjectToObject(unparser_s *unparser, char *key) {
	if(_jwObj( unparser, key ) == JSON_ERROR_OK) {
		jwPutch( unparser, '{' );
		jwPush( unparser, JSON_OBJECT );
	}
}

void addArrayToObject(unparser_s *unparser, char *key) {
	if(_jwObj( unparser, key ) == JSON_ERROR_OK) {
		jwPutch( unparser, '[' );
		jwPush( unparser, JSON_ARRAY );
	}
}

void addRawTextToArray(unparser_s *unparser, char *rawtext) {
	if(_jwArr( unparser ) == JSON_ERROR_OK) jwPutraw( unparser, rawtext);
}

void addStringToArray(unparser_s *unparser, char *value) {
	if(_jwArr( unparser ) == JSON_ERROR_OK) jwPutstr( unparser, value );
}

void addIntegerToArray(unparser_s *unparser, int value) {
	modp_itoa10( value, unparser->tmpbuf );
	addRawTextToArray( unparser, unparser->tmpbuf );
}

void addDoubleToArray(unparser_s *unparser, double value) {
	modp_dtoa2( value, unparser->tmpbuf, 6 );
	addRawTextToArray( unparser, unparser->tmpbuf );
}

void addBooleanToArray(unparser_s *unparser, int oneOrZero) {
	addRawTextToArray( unparser,  (oneOrZero) ? "true" : "false" );
}

void addNullToArray(unparser_s *unparser) {
	addRawTextToArray( unparser,  "null" );
}

void addObjectToArray(unparser_s *unparser) {
	if(_jwArr( unparser ) == JSON_ERROR_OK) {
		jwPutch( unparser, '{' );
		jwPush( unparser, JSON_OBJECT );
	}
}

void addArrayToArray(unparser_s *unparser) {
	if(_jwArr( unparser ) == JSON_ERROR_OK) {
		jwPutch( unparser, '[' );
		jwPush( unparser, JSON_ARRAY );
	}
}

char *errorToString(int err) {
	switch(err) {
		case JSON_ERROR_NOMEM:			return "Not enough tokens were provided.";
		case JSON_ERROR_INVAL:			return "Invalid character inside JSON string.";
		case JSON_ERROR_PART:			return "The string is not a full JSON packet, more bytes expected.";
		case JSON_ERROR_OK:       		return "OK"; 
		case JSON_ERROR_BUF_FULL:  	return "Output buffer full.";
		case JSON_ERROR_NOT_ARRAY:		return "Tried to write Array value into Object.";
		case JSON_ERROR_NOT_OBJECT:	return "Tried to write Object key/value into Array.";
		case JSON_ERROR_STACK_FULL:	return "Array/object nesting > JSON_STACK_DEPTH.";
		case JSON_ERROR_STACK_EMPTY:	return "Stack underflow error (too many 'end's).";
		case JSON_ERROR_NEST_ERROR:	return "Nesting error, not all objects closed when endUnparsingJSON() called.";
	}
	return "Unknown error.";
}
