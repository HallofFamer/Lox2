#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "token.h"
#include "../common/buffer.h"

const char* tokenNames[] = {
    [TOKEN_KIND_LEFT_PAREN]     = "TOKEN_LEFT_PAREN",
    [TOKEN_KIND_RIGHT_PAREN]    = "TOKEN_RIGHT_PAREN",
    [TOKEN_KIND_LEFT_BRACKET]   = "TOKEN_LEFT_BRACKET",
    [TOKEN_KIND_RIGHT_BRACKET]  = "TOKEN_RIGHT_BRACKET",
    [TOKEN_KIND_LEFT_BRACE]     = "TOKEN_LEFT_BRACE",
    [TOKEN_KIND_RIGHT_BRACE]    = "TOKEN_RIGHT_BRACE",
    [TOKEN_KIND_COLON]          = "TOKEN_COLON",
    [TOKEN_KIND_COMMA]          = "TOKEN_COMMA",
    [TOKEN_KIND_MINUS]          = "TOKEN_MINUS",
    [TOKEN_KIND_MODULO]         = "TOKEN_MODULO",
    [TOKEN_KIND_PIPE]           = "TOKEN_PIPE",
    [TOKEN_KIND_PLUS]           = "TOKEN_PLUS",
    [TOKEN_KIND_QUESTION]       = "TOKEN_QUESTION",
    [TOKEN_KIND_SEMICOLON]      = "TOKEN_SEMICOLON",
    [TOKEN_KIND_SLASH]          = "TOKEN_SLASH",
    [TOKEN_KIND_STAR]           = "TOKEN_STAR", 
    [TOKEN_KIND_BANG]           = "TOKEN_BANG",
    [TOKEN_KIND_BANG_EQUAL]     = "TOKEN_BANG_EQUAL",
    [TOKEN_KIND_EQUAL]          = "TOKEN_EQUAL",
    [TOKEN_KIND_EQUAL_EQUAL]    = "TOKEN_EQUAL_EQUAL",
    [TOKEN_KIND_GREATER]        = "TOKEN_GREATER",
    [TOKEN_KIND_GREATER_EQUAL]  = "TOKEN_GREATER_EQUAL",
    [TOKEN_KIND_LESS]           = "TOKEN_LESS",
    [TOKEN_KIND_LESS_EQUAL]     = "TOKEN_LESS_EQUAL",
    [TOKEN_KIND_DOT]            = "TOKEN_DOT",
    [TOKEN_KIND_DOT_DOT]        = "TOKEN_DOT_DOT",
    [TOKEN_KIND_IDENTIFIER]     = "TOKEN_IDENTIFIER",
    [TOKEN_KIND_STRING]         = "TOKEN_STRING",
    [TOKEN_KIND_INTERPOLATION]  = "TOKEN_INTERPOLATION",
    [TOKEN_KIND_NUMBER]         = "TOKEN_NUMBER",
    [TOKEN_KIND_INT]            = "TOKEN_INT",
    [TOKEN_KIND_AND]            = "TOKEN_AND",
    [TOKEN_KIND_AS]             = "TOKEN_AS",
    [TOKEN_KIND_ASYNC]          = "TOKEN_ASYNC",
    [TOKEN_KIND_AWAIT]          = "TOKEN_AWAIT",
    [TOKEN_KIND_BREAK]          = "TOKEN_BREAK",
    [TOKEN_KIND_CASE]           = "TOKEN_CASE",
    [TOKEN_KIND_CATCH]          = "TOKEN_CATCH",
    [TOKEN_KIND_CLASS]          = "TOKEN_CLASS",
    [TOKEN_KIND_CONTINUE]       = "TOKEN_CONTINUE",
    [TOKEN_KIND_DEFAULT]        = "TOKEN_DEFAULT",
    [TOKEN_KIND_ELSE]           = "TOKEN_ELSE",
    [TOKEN_KIND_EXTENDS]        = "TOKEN_EXTENDS",
    [TOKEN_KIND_FALSE]          = "TOKEN_FALSE",
    [TOKEN_KIND_FINALLY]        = "TOKEN_FINALLY",
    [TOKEN_KIND_FOR]            = "TOKEN_FOR",
    [TOKEN_KIND_FROM]           = "TOKEN_FROM",
    [TOKEN_KIND_FUN]            = "TOKEN_FUN",
    [TOKEN_KIND_IF]             = "TOKEN_IF",
    [TOKEN_KIND_NAMESPACE]      = "TOKEN_NAMESPACE",
    [TOKEN_KIND_NIL]            = "TOKEN_NIL",
    [TOKEN_KIND_OR]             = "TOKEN_OR",
    [TOKEN_KIND_REQUIRE]        = "TOKEN_REQUIRE",
    [TOKEN_KIND_RETURN]         = "TOKEN_RETURN",
    [TOKEN_KIND_SUPER]          = "TOKEN_SUPER",
    [TOKEN_KIND_SWITCH]         = "TOKEN_SWITCH",
    [TOKEN_KIND_THIS]           = "TOKEN_THIS",
    [TOKEN_KIND_THROW]          = "TOKEN_THROW",
    [TOKEN_KIND_TRAIT]          = "TOKEN_TRAIT",
    [TOKEN_KIND_TRUE]           = "TOKEN_TRUE",
    [TOKEN_KIND_TRY]            = "TOKEN_TRY",
    [TOKEN_KIND_TYPE]           = "TOKEN_TYPE",
    [TOKEN_KIND_USING]          = "TOKEN_USING",
    [TOKEN_KIND_VAL]            = "TOKEN_VAL",
    [TOKEN_KIND_VAR]            = "TOKEN_VAR", 
    [TOKEN_KIND_VOID]           = "TOKEN_VOID",
    [TOKEN_KIND_WHILE]          = "TOKEN_WHILE",
    [TOKEN_KIND_WITH]           = "TOKEN_WITH",
    [TOKEN_KIND_YIELD]          = "TOKEN_YIELD",
    [TOKEN_KIND_ERROR]          = "TOKEN_ERROR", 
    [TOKEN_KIND_EMPTY]          = "TOKEN_EMPTY",
    [TOKEN_KIND_NEW_LINE]       = "TOKEN_NEW_LINE",
    [TOKEN_KIND_EOF]            = "TOKEN_EOF"
};

Token syntheticToken(const char* text) {
    return (Token) {
        .type = TOKEN_KIND_EMPTY,
        .start = text,
        .length = (int)strlen(text),
        .line = 0
    };
}

Token syntheticTokenAtLine(const char* text, int line) {
	return (Token) {
        .type = TOKEN_KIND_EMPTY, 
        .start = text, 
        .length = (int)strlen(text),
        .line = line
    };
}

bool tokensEqual(Token* token, Token* token2) {
    if (token->length != token2->length) return false;
    return memcmp(token->start, token2->start, token->length) == 0;
}

bool tokenIsLiteral(Token token) {
    switch (token.type) {
        case TOKEN_KIND_NIL:
        case TOKEN_KIND_TRUE:
        case TOKEN_KIND_FALSE:
        case TOKEN_KIND_NUMBER:
        case TOKEN_KIND_INT:
        case TOKEN_KIND_STRING:
            return true;
        default:
            return false;
    }
}

bool tokenIsOperator(Token token) {
    switch (token.type) {
        case TOKEN_KIND_EQUAL_EQUAL:
        case TOKEN_KIND_GREATER:
        case TOKEN_KIND_LESS:
        case TOKEN_KIND_PLUS:
        case TOKEN_KIND_MINUS:
        case TOKEN_KIND_STAR:
        case TOKEN_KIND_SLASH:
        case TOKEN_KIND_MODULO:
        case TOKEN_KIND_DOT_DOT:
        case TOKEN_KIND_LEFT_BRACKET:
            return true;
        default:
            return false;
    }
}

char* tokenToCString(Token token) {
    char* buffer = bufferNewCString((size_t)token.length);
    if (buffer != NULL) {
        if (token.length > 0) memcpy(buffer, token.start, token.length);
        buffer[token.length] = '\0';
        return buffer;
    }
    else {
        fprintf(stderr, "Not enough memory to convert token to string.");
        return NULL;
    }
}

void outputToken(Token token) {
    printf("Scanning Token type %s at line %d\n", tokenNames[token.type], token.line);
}

void initTokenStream(TokenStream* tokenStream) {
    tokenStream->elements = NULL;
    tokenStream->capacity = 0;
    tokenStream->count = 0;
}

void freeTokenStream(TokenStream* tokenStream) {
    free(tokenStream->elements); 
    free(tokenStream);
}

void tokenStreamAdd(TokenStream* tokens, Token token) {
    if (tokens->capacity < tokens->count + 1) {
        int oldCapacity = tokens->capacity; 
        tokens->capacity = bufferGrowCapacity(oldCapacity); 
        Token* elements = (Token*)realloc(tokens->elements, sizeof(Token) * tokens->capacity); 
        
        if (elements != NULL) tokens->elements = elements; 
        else exit(1);
    } 
    
    tokens->elements[tokens->count] = token; 
    tokens->count++;
}

Token tokenStreamDelete(TokenStream* tokens, int index) {
    Token token = tokens->elements[index]; 
    for (int i = index; i < tokens->count - 1; i++) {
        tokens->elements[i] = tokens->elements[i + 1];
    } 
    tokens->count--; 
    return token;
}

void outputTokenStream(TokenStream* tokens) {
    for (int i = 0; i < tokens->count; i++) {
        outputToken(tokens->elements[i]);
    }
}