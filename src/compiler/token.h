#pragma once
#ifndef clox_token_h
#define clox_token_h

#include "../common/buffer.h"
#include "../common/common.h"

typedef enum {
    TOKEN_SYMBOL_LEFT_PAREN, TOKEN_SYMBOL_RIGHT_PAREN,
    TOKEN_SYMBOL_LEFT_BRACKET, TOKEN_SYMBOL_RIGHT_BRACKET,
    TOKEN_SYMBOL_LEFT_BRACE, TOKEN_SYMBOL_RIGHT_BRACE,
    TOKEN_SYMBOL_COLON, TOKEN_SYMBOL_COMMA,
    TOKEN_SYMBOL_MINUS, TOKEN_SYMBOL_MODULO,
    TOKEN_SYMBOL_PIPE, TOKEN_SYMBOL_PLUS,
    TOKEN_SYMBOL_QUESTION, TOKEN_SYMBOL_SEMICOLON,
    TOKEN_SYMBOL_SLASH, TOKEN_SYMBOL_STAR,

    TOKEN_BANG, TOKEN_BANG_EQUAL,
    TOKEN_EQUAL, TOKEN_EQUAL_EQUAL,
    TOKEN_GREATER, TOKEN_GREATER_EQUAL,
    TOKEN_LESS, TOKEN_LESS_EQUAL,
    TOKEN_DOT, TOKEN_DOT_DOT,

    TOKEN_IDENTIFIER, TOKEN_STRING, TOKEN_INTERPOLATION, TOKEN_NUMBER, TOKEN_INT,

    TOKEN_AND, TOKEN_AS, TOKEN_ASYNC, TOKEN_AWAIT, TOKEN_BREAK,
    TOKEN_CASE, TOKEN_CATCH, TOKEN_CLASS, TOKEN_CONTINUE, TOKEN_DEFAULT, 
    TOKEN_ELSE, TOKEN_EXTENDS, TOKEN_FALSE, TOKEN_FINALLY, TOKEN_FOR,
    TOKEN_FROM, TOKEN_FUN, TOKEN_IF, TOKEN_NAMESPACE, TOKEN_NIL, TOKEN_OR,
    TOKEN_REQUIRE, TOKEN_RETURN, TOKEN_SUPER, TOKEN_SWITCH, TOKEN_THIS,
    TOKEN_THROW, TOKEN_TRAIT, TOKEN_TRUE, TOKEN_TRY, TOKEN_TYPE_, TOKEN_USING,
    TOKEN_VAL, TOKEN_VAR, TOKEN_VOID, TOKEN_WHILE, TOKEN_WITH, TOKEN_YIELD,

    TOKEN_ERROR, TOKEN_EMPTY, TOKEN_NEW_LINE, TOKEN_EOF
} TokenSymbol;

typedef struct {
    TokenSymbol type;
    const char* start;
    int length;
    int line;
} Token;

typedef struct {
    Token* elements;
    int capacity;
    int count;
} TokenStream;

Token syntheticToken(const char* text);
bool tokensEqual(Token* token, Token* token2);
bool tokenIsLiteral(Token token);
bool tokenIsOperator(Token token);
char* tokenToCString(Token token);
void outputToken(Token token);
void initTokenStream(TokenStream* tokens);
void freeTokenStream(TokenStream* tokens);
void tokenStreamAdd(TokenStream* tokens, Token token);
Token tokenStreamDelete(TokenStream* tokens, int index);
void outputTokenStream(TokenStream* tokens);

static inline Token emptyToken() {
    return syntheticToken("");
}

#endif // !clox_token_h