#pragma once
#ifndef clox_parser_h
#define clox_parser_h

#include <setjmp.h>

#include "ast.h"
#include "lexer.h"

typedef struct {
    TokenStream* tokens;
    int index;
    Token previous;
    Token current;
    Token next;
    Token rootClass;
    bool debugAst;
    bool hadError;
    bool panicMode;
    jmp_buf jumpBuffer;
} Parser;

void initParser(Parser* parser, TokenStream* tokens, bool debugAst);
Ast* parse(Parser* parser);

#endif // !clox_parser_h
