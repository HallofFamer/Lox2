#pragma once
#ifndef clox_token_h
#define clox_token_h

#include "../common/buffer.h"
#include "../common/common.h"

typedef enum {
    TOKEN_KIND_LEFT_PAREN, TOKEN_KIND_RIGHT_PAREN,
    TOKEN_KIND_LEFT_BRACKET, TOKEN_KIND_RIGHT_BRACKET,
    TOKEN_KIND_LEFT_BRACE, TOKEN_KIND_RIGHT_BRACE,
    TOKEN_KIND_COLON, TOKEN_KIND_COMMA,
    TOKEN_KIND_MINUS, TOKEN_KIND_MODULO,
    TOKEN_KIND_PIPE, TOKEN_KIND_PLUS,
    TOKEN_KIND_QUESTION, TOKEN_KIND_SEMICOLON,
    TOKEN_KIND_SLASH, TOKEN_KIND_STAR,

    TOKEN_KIND_BANG, TOKEN_KIND_BANG_EQUAL,
    TOKEN_KIND_EQUAL, TOKEN_KIND_EQUAL_EQUAL,
    TOKEN_KIND_GREATER, TOKEN_KIND_GREATER_EQUAL,
    TOKEN_KIND_LESS, TOKEN_KIND_LESS_EQUAL,
    TOKEN_KIND_DOT, TOKEN_KIND_DOT_DOT,

    TOKEN_KIND_IDENTIFIER, TOKEN_KIND_STRING, TOKEN_KIND_INTERPOLATION, TOKEN_KIND_NUMBER, TOKEN_KIND_INT,

    TOKEN_KIND_AND, TOKEN_KIND_AS, TOKEN_KIND_ASYNC, TOKEN_KIND_AWAIT, TOKEN_KIND_BREAK,
    TOKEN_KIND_CASE, TOKEN_KIND_CATCH, TOKEN_KIND_CLASS, TOKEN_KIND_CONTINUE, TOKEN_KIND_DEFAULT, 
    TOKEN_KIND_ELSE, TOKEN_KIND_EXTENDS, TOKEN_KIND_FALSE, TOKEN_KIND_FINALLY, TOKEN_KIND_FOR,
    TOKEN_KIND_FROM, TOKEN_KIND_FUN, TOKEN_KIND_IF, TOKEN_KIND_NAMESPACE, TOKEN_KIND_NIL, TOKEN_KIND_OR,
    TOKEN_KIND_REQUIRE, TOKEN_KIND_RETURN, TOKEN_KIND_SUPER, TOKEN_KIND_SWITCH, TOKEN_KIND_THIS,
    TOKEN_KIND_THROW, TOKEN_KIND_TRAIT, TOKEN_KIND_TRUE, TOKEN_KIND_TRY, TOKEN_KIND_TYPE, TOKEN_KIND_USING,
    TOKEN_KIND_VAL, TOKEN_KIND_VAR, TOKEN_KIND_VOID, TOKEN_KIND_WHILE, TOKEN_KIND_WITH, TOKEN_KIND_YIELD,

    TOKEN_SYMBOL_ERROR, TOKEN_SYMBOL_EMPTY, TOKEN_SYMBOL_NEW_LINE, TOKEN_SYMBOL_EOF
} TokenKind;

typedef struct {
    TokenKind type;
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
Token syntheticTokenAtLine(const char* text, int line);
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