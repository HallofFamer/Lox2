#pragma once
#ifndef clox_parser_h
#define clox_parser_h

#include <setjmp.h>

#include "ast.h"

typedef struct {
    TokenStream* tokens;
    int index;
    Token current;
    Token rootClass;
    bool debugAst;
    bool hadError;
    bool panicMode;
    jmp_buf jumpBuffer;
} Parser;

void initParser(Parser* parser, TokenStream* tokens, bool debugAst);
Ast* parse(Parser* parser);

#endif // !clox_parser_h
