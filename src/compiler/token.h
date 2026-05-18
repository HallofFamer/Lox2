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

    TOKEN_SYMBOL_AND, TOKEN_SYMBOL_AS, TOKEN_SYMBOL_ASYNC, TOKEN_SYMBOL_AWAIT, TOKEN_SYMBOL_BREAK,
    TOKEN_SYMBOL_CASE, TOKEN_SYMBOL_CATCH, TOKEN_SYMBOL_CLASS, TOKEN_SYMBOL_CONTINUE, TOKEN_SYMBOL_DEFAULT, 
    TOKEN_SYMBOL_ELSE, TOKEN_SYMBOL_EXTENDS, TOKEN_SYMBOL_FALSE, TOKEN_SYMBOL_FINALLY, TOKEN_SYMBOL_FOR,
    TOKEN_SYMBOL_FROM, TOKEN_SYMBOL_FUN, TOKEN_SYMBOL_IF, TOKEN_SYMBOL_NAMESPACE, TOKEN_SYMBOL_NIL, TOKEN_SYMBOL_OR,
    TOKEN_SYMBOL_REQUIRE, TOKEN_SYMBOL_RETURN, TOKEN_SYMBOL_SUPER, TOKEN_SYMBOL_SWITCH, TOKEN_SYMBOL_THIS,
    TOKEN_SYMBOL_THROW, TOKEN_SYMBOL_TRAIT, TOKEN_SYMBOL_TRUE, TOKEN_SYMBOL_TRY, TOKEN_SYMBOL_TYPE, TOKEN_SYMBOL_USING,
    TOKEN_SYMBOL_VAL, TOKEN_SYMBOL_VAR, TOKEN_SYMBOL_VOID, TOKEN_SYMBOL_WHILE, TOKEN_SYMBOL_WITH, TOKEN_SYMBOL_YIELD,

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