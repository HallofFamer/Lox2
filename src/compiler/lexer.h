#pragma once
#ifndef clox_lexer_h
#define clox_lexer_h

#include "token.h"

typedef struct {
    const char* start;
    const char* current;
    int line;
    int interpolationDepth;
    bool debugToken;
    bool hadError;
} Lexer;

void initLexer(Lexer* lexer, const char* source, bool debugToken);
Token scanToken(Lexer* lexer);
TokenStream* lex(Lexer* lexer);

#endif // !clox_lexer_h