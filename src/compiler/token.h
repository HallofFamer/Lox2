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

    TOKEN_SYMBOL_BANG, TOKEN_SYMBOL_BANG_EQUAL,
    TOKEN_SYMBOL_EQUAL, TOKEN_SYMBOL_EQUAL_EQUAL,
    TOKEN_SYMBOL_GREATER, TOKEN_SYMBOL_GREATER_EQUAL,
    TOKEN_SYMBOL_LESS, TOKEN_SYMBOL_LESS_EQUAL,
    TOKEN_SYMBOL_DOT, TOKEN_SYMBOL_DOT_DOT,

    TOKEN_SYMBOL_IDENTIFIER, TOKEN_SYMBOL_STRING, TOKEN_SYMBOL_INTERPOLATION, TOKEN_SYMBOL_NUMBER, TOKEN_SYMBOL_INT,

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