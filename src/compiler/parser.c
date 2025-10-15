#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parser.h"
#include "../inc/utf8.h"

typedef enum {
    PREC_NONE,
    PREC_ASSIGNMENT,
    PREC_OR,
    PREC_AND,
    PREC_EQUALITY,
    PREC_COMPARISON,
    PREC_TERM,
    PREC_FACTOR,
    PREC_UNARY,
    PREC_CALL,
    PREC_PRIMARY
} Precedence;

typedef Ast* (*ParsePrefixFn)(Parser* parser, Token token, bool canAssign);
typedef Ast* (*ParseInfixFn)(Parser* parser, Token token, Ast* left, bool canAssign);

typedef struct {
    ParsePrefixFn prefix;
    ParseInfixFn infix;
    Precedence precedence;
    bool startExpr;
} ParseRule;

static Token currentToken(Parser* parser) {
    return parser->current;
}

static TokenSymbol currentTokenType(Parser* parser) {
    return parser->current.type;
}

static Token previousToken(Parser* parser) {
    Token previousToken = parser->tokens->elements[parser->index - 1];
    if (previousToken.type == TOKEN_NEW_LINE) return parser->tokens->elements[parser->index - 2];
    return previousToken;
}

static TokenSymbol previousTokenType(Parser* parser) {
    return previousToken(parser).type;
}

static Token nextToken(Parser* parser) {
    Token nextToken = parser->tokens->elements[parser->index + 1];
    if (nextToken.type == TOKEN_NEW_LINE) return parser->tokens->elements[parser->index + 2];
    return nextToken;
}

static TokenSymbol nextTokenType(Parser* parser) {
    return nextToken(parser).type;
}

static bool newLineBeforePrevious(Parser* parser) {
    return (parser->tokens->elements[parser->index - 1].type == TOKEN_NEW_LINE || parser->tokens->elements[parser->index - 2].type == TOKEN_NEW_LINE);
}

static bool newLineAfterCurrent(Parser* parser) {
    return (parser->tokens->elements[parser->index + 1].type == TOKEN_NEW_LINE);
}

static void parseError(Parser* parser, Token token, const char* message) {
    if (parser->panicMode) return;
    parser->panicMode = true;
    fprintf(stderr, "[line %d] Parse Error", token.line);

    if (token.type == TOKEN_EOF) fprintf(stderr, " at end");
    else fprintf(stderr, " at '%.*s'", token.length, token.start);

    fprintf(stderr, ": %s\n", message);
    parser->hadError = true;
    longjmp(parser->jumpBuffer, 1);
}

static void parseErrorAtPrevious(Parser* parser, const char* message) {
    parseError(parser, previousToken(parser), message);
}

static void parseErrorAtCurrent(Parser* parser, const char* message) {
    parseError(parser, parser->current, message);
}

static void advance(Parser* parser) {
    parser->current = parser->tokens->elements[++parser->index];
    if (parser->current.type == TOKEN_NEW_LINE) {
        parser->current = parser->tokens->elements[++parser->index];
    }
}

static void backtrack(Parser* parser) {
    if (parser->index > 0) {
        parser->current = parser->tokens->elements[--parser->index];
        if (parser->current.type == TOKEN_NEW_LINE) {
            parser->current = parser->tokens->elements[--parser->index];
        }
    }
}

static bool resetIndex(Parser* parser, int index, Token current, bool value) {
    parser->index = index;
    parser->current = current;
    return value;
}

static void consume(Parser* parser, TokenSymbol type, const char* message) {
    if (parser->current.type == type) {
        advance(parser);
        return;
    }
    parseErrorAtCurrent(parser, message);
}

static void consumerTerminator(Parser* parser, const char* message) {
    if (parser->current.type == TOKEN_SEMICOLON) {
        advance(parser);
        return;
    }
    else if (newLineBeforePrevious(parser) || parser->current.type == TOKEN_RIGHT_BRACE || parser->current.type == TOKEN_EOF) {
        return;
    }
    parseErrorAtCurrent(parser, message);
}

static bool check(Parser* parser, TokenSymbol type) {
    return currentTokenType(parser) == type;
}

static bool checkNext(Parser* parser, TokenSymbol type) {
    return nextTokenType(parser) == type;
}

static bool checkEither(Parser* parser, TokenSymbol type1, TokenSymbol type2) {
    return check(parser, type1) || check(parser, type2);
}

static bool checkBoth(Parser* parser, TokenSymbol type) {
    return check(parser, type) && checkNext(parser, type);
}

static bool match(Parser* parser, TokenSymbol type) {
    if (!check(parser, type)) return false;
    advance(parser);
    return true;
}

static int hexDigit(Parser* parser, char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    parseErrorAtPrevious(parser, "Invalid hex escape sequence.");
    return -1;
}

static int hexEscape(Parser* parser, const char* source, int base, int startIndex) {
    int value = 0;
    for (int i = 0; i < base; i++) {
        int index = startIndex + i + 2;
        if (source[index] == '"' || source[index] == '\0') {
            parseErrorAtPrevious(parser, "Incomplete hex escape sequence.");
            break;
        }

        int digit = hexDigit(parser, source[index]);
        if (digit == -1) break;
        value = (value * 16) | digit;
    }
    return value;
}

static int unicodeEscape(Parser* parser, const char* source, char* target, int base, int startIndex, int currentLength) {
    int value = hexEscape(parser, source, base, startIndex);
    int numBytes = utf8_num_bytes(value);
    if (numBytes < 0) parseErrorAtPrevious(parser, "Negative unicode character specified.");
    if (value > 0xffff) numBytes++;

    if (numBytes > 0) {
        char* utfChar = utf8_encode(value);
        if (utfChar == NULL) parseErrorAtPrevious(parser, "Invalid unicode character specified.");
        else {
            memcpy(target + currentLength, utfChar, (size_t)numBytes + 1);
            free(utfChar);
        }
    }
    return numBytes;
}

static void* resizeString(char* string, size_t newSize) {
    char* result = (char*)realloc(string, newSize);
    if (result == NULL) exit(1);
    return result;
}

static char* parseString(Parser* parser, int* length) {
    int maxLength = previousToken(parser).length - 2;
    const char* source = previousToken(parser).start + 1;
    char* target = (char*)malloc((size_t)maxLength + 1);
    if (target == NULL) {
        fprintf(stderr, "Not enough memory to parser string token.");
        exit(1);
    }

    int i = 0, j = 0;
    while (i < maxLength) {
        if (source[i] == '\\') {
            switch (source[i + 1]) {
                case 'a': {
                    target[j] = '\a';
                    i++;
                    break;
                }
                case 'b': {
                    target[j] = '\b';
                    i++;
                    break;
                }
                case 'f': {
                    target[j] = '\f';
                    i++;
                    break;
                }
                case 'n': {
                    target[j] = '\n';
                    i++;
                    break;
                }
                case 'r': {
                    target[j] = '\r';
                    i++;
                    break;
                }
                case 't': {
                    target[j] = '\t';
                    i++;
                    break;
                }    
                case 'u': {
                    int numBytes = unicodeEscape(parser, source, target, 4, i, j);
                    j += numBytes > 4 ? numBytes - 2 : numBytes - 1;
                    i += numBytes > 4 ? 6 : 5;
                    break;
                }
                case 'U': {
                    int numBytes = unicodeEscape(parser, source, target, 8, i, j);
                    j += numBytes > 4 ? numBytes - 2 : numBytes - 1;
                    i += 9;
                    break;
                }
                case 'v': {
                    target[j] = '\v';
                    i++;
                    break;
                }
                case 'x': {
                    target[j] = hexEscape(parser, source, 2, i);
                    i += 3;
                    break;
                }
                case '"': {
                    target[j] = '"';
                    i++;
                    break;
                }
                case '\\': {
                    target[j] = '\\';
                    i++;
                    break;
                }
                default: 
                    target[j] = source[i];
            }
        }
        else {
            target[j] = source[i];
        }

        i++;
        j++;
    }

    target = resizeString(target, (size_t)j + 1);
    target[j] = '\0';
    *length = j;
    return target;
}

static void synchronize(Parser* parser) {
    parser->panicMode = false;

    while (parser->current.type != TOKEN_EOF) {
        if (previousTokenType(parser) == TOKEN_SEMICOLON) return;

        switch (parser->current.type) {
            case TOKEN_ASYNC:
            case TOKEN_AWAIT:
            case TOKEN_CLASS:
            case TOKEN_FOR:
            case TOKEN_FROM:
            case TOKEN_FUN:
            case TOKEN_IF:
            case TOKEN_NAMESPACE:
            case TOKEN_RETURN:
            case TOKEN_SWITCH:
            case TOKEN_TRAIT:
            case TOKEN_THROW:
            case TOKEN_USING:
            case TOKEN_VAL:
            case TOKEN_VAR:
            case TOKEN_VOID:
            case TOKEN_WHILE:
            case TOKEN_YIELD:
                return;

            default: ;
        }
        advance(parser);
    }
}

static Ast* expression(Parser* parser);
static Ast* variable(Parser* parser, Token token, bool canAssign);
static Ast* statement(Parser* parser);
static Ast* block(Parser* parser);
static Ast* function(Parser* parser, Ast* returnType, bool isAsync, bool isLambda, bool isVoid);
static Ast* declaration(Parser* parser);
static Ast* funDeclaration(Parser* parser, bool isAsync, bool hasReturnType);
static ParseRule* getRule(TokenSymbol type);
static Ast* parsePrecedence(Parser* parser, Precedence precedence);

static Ast* argumentList(Parser* parser) {
    Ast* argList = emptyAst(AST_LIST_EXPR, previousToken(parser));
    uint8_t argCount = 0;

    if (!check(parser, TOKEN_RIGHT_PAREN)) {
        do {
            Ast* child = expression(parser);
            if (argCount == UINT8_MAX) parseErrorAtPrevious(parser, "Can't have more than 255 arguments.");
            astAppendChild(argList, child);
            argCount++;
        } while (match(parser, TOKEN_COMMA));
    }

    consume(parser, TOKEN_RIGHT_PAREN, "Expect ')' after arguments.");
    return argList;
}

static Ast* identifier(Parser* parser, const char* message) {
    consume(parser, TOKEN_IDENTIFIER, message);
    return emptyAst(AST_EXPR_VARIABLE, previousToken(parser));
}

static Token identifierToken(Parser* parser, const char* message) {
    switch (parser->current.type) {
        case TOKEN_IDENTIFIER:
        case TOKEN_EQUAL_EQUAL:
        case TOKEN_GREATER:
        case TOKEN_LESS:
        case TOKEN_PLUS:
        case TOKEN_MINUS:
        case TOKEN_STAR:
        case TOKEN_SLASH:
        case TOKEN_MODULO:
        case TOKEN_DOT_DOT:
            advance(parser);
            return previousToken(parser);
        case TOKEN_LEFT_BRACKET:
            advance(parser);
            if (match(parser, TOKEN_RIGHT_BRACKET)) {
                return syntheticToken(match(parser, TOKEN_EQUAL) ? "[]=" : "[]");
            }
        case TOKEN_LEFT_PAREN:
            advance(parser);
            if (match(parser, TOKEN_RIGHT_PAREN)) {
                return syntheticToken("()");
            }
        default:
            break;
    }

    parseErrorAtCurrent(parser, message);
    return previousToken(parser);
}

static Ast* and_(Parser* parser, Token token, Ast* left, bool canAssign) {
    ParseRule* rule = getRule(previousTokenType(parser));
    Ast* right = parsePrecedence(parser, PREC_AND);
    return newAst(AST_EXPR_AND, token, 2, left, right);
}

static Ast* binary(Parser* parser, Token token, Ast* left, bool canAssign) {
    ParseRule* rule = getRule(previousTokenType(parser));
    Ast* right = parsePrecedence(parser, (Precedence)(rule->precedence + 1));
    return newAst(AST_EXPR_BINARY, token, 2, left, right);
}

static Ast* call(Parser* parser, Token token, Ast* left, bool canAssign) { 
    Ast* right = argumentList(parser);
    return newAst(AST_EXPR_CALL, token, 2, left, right);
}

static Ast* dot(Parser* parser, Token token, Ast* left, bool canAssign) { 
    Token property = identifierToken(parser, "Expect property name after '.'.");

    if (canAssign && match(parser, TOKEN_EQUAL)) {
        Ast* right = expression(parser);
        return newAst(AST_EXPR_PROPERTY_SET, property, 2, left, right);
    }
    else if (match(parser, TOKEN_LEFT_PAREN)) {
        Ast* right = argumentList(parser);
        return newAst(AST_EXPR_INVOKE, property, 2, left, right);
    }
    else {
        return newAst(AST_EXPR_PROPERTY_GET, property, 1, left);
    }
}

static Ast* nil(Parser* parser, Token token, Ast* left, bool canAssign) {
    Ast* right = parsePrecedence(parser, PREC_PRIMARY);
    return newAst(AST_EXPR_NIL, token, 2, left, right);
}

static Ast* or_(Parser* parser, Token token, Ast* left, bool canAssign) {
    ParseRule* rule = getRule(previousTokenType(parser));
    Ast* right = parsePrecedence(parser, PREC_OR);
    return newAst(AST_EXPR_OR, token, 2, left, right);
}

static Ast* subscript(Parser* parser, Token token, Ast* left, bool canAssign) {
    Ast* index = expression(parser);
    consume(parser, TOKEN_RIGHT_BRACKET, "Expect ']' after subscript.");

    if (canAssign && match(parser, TOKEN_EQUAL)) {
        Ast* right = expression(parser);
        return newAst(AST_EXPR_SUBSCRIPT_SET, token, 3, left, index, right);
    }
    else {
        return newAst(AST_EXPR_SUBSCRIPT_GET, token, 2, left, index);
    }
}

static Ast* question(Parser* parser, Token token, Ast* left, bool canAssign) { 
    Ast* expr = NULL;

    if (match(parser, TOKEN_DOT)) {
        expr = dot(parser, token, left, canAssign);
    }
    else if (match(parser, TOKEN_LEFT_BRACKET)) {
        expr = subscript(parser, token, left, canAssign);
    }
    else if (match(parser, TOKEN_LEFT_PAREN)) {
        expr = call(parser, token, left, canAssign);
    }
    else if (match(parser, TOKEN_QUESTION) || match(parser, TOKEN_COLON)) {
        expr = nil(parser, previousToken(parser), left, canAssign);
    }

    if (expr != NULL) expr->attribute.isOptional = true;
    return expr;
}

static Ast* literal(Parser* parser, Token token, bool canAssign) {
    return emptyAst(AST_EXPR_LITERAL, token);
}

static Ast* grouping(Parser* parser, Token token, bool canAssign) { 
    Ast* expr = emptyAst(AST_EXPR_GROUPING, token);
    Ast* child = expression(parser);
    astAppendChild(expr, child);
    consume(parser, TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
    return expr;
}

static Ast* string(Parser* parser, Token token, bool canAssign) {
    int length = 0;
    char* str = parseString(parser, &length);
    Token strToken = {
        .length = length,
        .line = token.line,
        .start = str,
        .type = TOKEN_STRING
    };
    return emptyAst(AST_EXPR_LITERAL, strToken);
}

static Ast* interpolation(Parser* parser, Token token, bool canAssign) { 
    Ast* exprs = emptyAst(AST_LIST_EXPR, token);
    int count = 0;
    do {
        bool concatenate = false;
        bool isString = false;

        if (previousToken(parser).length > 2) {
            Ast* str = string(parser, previousToken(parser), false);
            concatenate = true;
            isString = true;
            astAppendChild(exprs, str);
        }

        Ast* expr = expression(parser);
        astAppendChild(exprs, expr);
        count++;
    } while (match(parser, TOKEN_INTERPOLATION));

    consume(parser, TOKEN_STRING, "Expect end of string interpolation.");
    if (previousToken(parser).length > 2) {
        Ast* str = string(parser, previousToken(parser), false);
        astAppendChild(exprs, str);
    }
    return newAst(AST_EXPR_INTERPOLATION, token, 1, exprs);
}

static Ast* array(Parser* parser, Token token, Ast* element) {
    Ast* elements = newAst(AST_LIST_EXPR, token, 1, element);
    uint8_t elementCount = 1;

    while (match(parser, TOKEN_COMMA)) {
        element = expression(parser);
        astAppendChild(elements, element);

        if (elementCount == UINT8_MAX) {
            parseErrorAtPrevious(parser, "Cannot have more than 255 elements.");
        }
        elementCount++;
    }

    consume(parser, TOKEN_RIGHT_BRACKET, "Expect ']' after elements.");
    return newAst(AST_EXPR_ARRAY, token, 1, elements);
}

static Ast* dictionary(Parser* parser, Token token, Ast* key, Ast* value) {
    Ast* keys = newAst(AST_LIST_EXPR, token, 1, key);
    Ast* values = newAst(AST_LIST_EXPR, token, 1, value);
    uint8_t entryCount = 1;

    while (match(parser, TOKEN_COMMA)) {
        Ast* key = expression(parser);
        astAppendChild(keys, key);
        consume(parser, TOKEN_COLON, "Expect ':' after entry key.");
        Ast* value = expression(parser);
        astAppendChild(values, value);

        if (entryCount == UINT8_MAX) {
            parseErrorAtPrevious(parser, "Cannot have more than 255 entries.");
        }
        entryCount++;
    }

    consume(parser, TOKEN_RIGHT_BRACKET, "Expect ']' after entries.");
    return newAst(AST_EXPR_DICTIONARY, token, 2, keys, values);
}

static Ast* collection(Parser* parser, Token token, bool canAssign) { 
    if (match(parser, TOKEN_RIGHT_BRACKET)) return emptyAst(AST_EXPR_ARRAY, token);
    else {
        Ast* first = expression(parser);
        if (match(parser, TOKEN_COLON)) {
            Ast* firstValue = expression(parser);
            return dictionary(parser, token, first, firstValue);
        }
        else return array(parser, token, first);
    }
}

static Ast* closure(Parser* parser, Token token, bool canAssign) {
    return function(parser, emptyAst(AST_EXPR_TYPE, emptyToken()), false, false, false);
}

static Ast* lambda(Parser* parser, Token token, bool canAssign) {
    return function(parser, emptyAst(AST_EXPR_TYPE, emptyToken()), false, true, false);
}

static Ast* behaviorType(Parser* parser, const char* message) {
    consume(parser, TOKEN_IDENTIFIER, message);
    return emptyAst(AST_EXPR_TYPE, previousToken(parser));
}

static Ast* callableType(Parser* parser, const char* message) {
    consume(parser, TOKEN_IDENTIFIER, message);
    Ast* returnType = emptyAst(AST_EXPR_TYPE, previousToken(parser));
    if (returnType->token.type == TOKEN_VOID) {
        returnType->attribute.isVoid = true;
    }
    
    consume(parser, TOKEN_FUN, "Expect 'fun' keyword after return type declaration.");
    Token token = previousToken(parser);
    consume(parser, TOKEN_LEFT_PAREN, "Expect '(' after 'fun' keyword.");
    Ast* paramTypes = emptyAst(AST_LIST_EXPR, previousToken(parser));
    Ast* paramType = NULL;
    int arity = 0;

    do {
        arity++;
        if (arity > UINT8_MAX) parseErrorAtCurrent(parser, "Can't have more than 255 param types.");
        if (checkEither(parser, TOKEN_IDENTIFIER, TOKEN_VOID) && checkNext(parser, TOKEN_FUN)) {
            paramType = callableType(parser, "Expect function type.");
            paramType->attribute.isFunction = true;
        }
        else paramType = behaviorType(parser, "Expect param type.");
        astAppendChild(paramTypes, paramType);
    } while (match(parser, TOKEN_COMMA));

    consume(parser, TOKEN_RIGHT_PAREN, "Expect ')' after param types.");
    Ast* type = newAst(AST_EXPR_TYPE, token, 2, returnType, paramTypes);
    type->attribute.isFunction = true;
    return type;
}

static Ast* metaclassType(Parser* parser, const char* message) {
    consume(parser, TOKEN_IDENTIFIER, message);
    Token token = previousToken(parser);
    consume(parser, TOKEN_CLASS, "Expect 'class' keyword after metaclass type declaration.");
    Ast* type = emptyAst(AST_EXPR_TYPE, token);
    type->attribute.isClass = true;
    return type;
}

static Ast* type_(Parser* parser, const char* message) {
    if (checkBoth(parser, TOKEN_IDENTIFIER)) return behaviorType(parser, message);
    else if (checkEither(parser, TOKEN_IDENTIFIER, TOKEN_VOID) && checkNext(parser, TOKEN_FUN)) return callableType(parser, message);
    else if (check(parser, TOKEN_IDENTIFIER) && checkNext(parser, TOKEN_CLASS)) return metaclassType(parser, message);
    else return emptyAst(AST_EXPR_TYPE, emptyToken());
}

static Ast* variable(Parser* parser, Token token, bool canAssign) {
    if (canAssign && match(parser, TOKEN_EQUAL)) {
        Ast* expr = expression(parser);
        return newAst(AST_EXPR_ASSIGN, token, 1, expr);
    }
    else if (check(parser, TOKEN_FUN) && checkNext(parser, TOKEN_LEFT_PAREN)) {
        Ast* returnType = emptyAst(AST_EXPR_TYPE, token);
        advance(parser);
        return function(parser, returnType, false, false, false);
    }
    return emptyAst(AST_EXPR_VARIABLE, token);
}

static Ast* parameter(Parser* parser, bool isLambda, const char* message) {
    bool isMutable = match(parser, TOKEN_VAR);
    Ast* type = isLambda ? emptyAst(AST_EXPR_TYPE, emptyToken()) : type_(parser, "Expect type declaration.");
    consume(parser, TOKEN_IDENTIFIER, message);

    Ast* param = newAst(AST_EXPR_PARAM, previousToken(parser), 1, type);
    param->attribute.isMutable = isMutable;
    return param;
}

static Ast* parameterList(Parser* parser, bool isLambda, Token token) {
    Ast* params = emptyAst(AST_LIST_VAR, token);
    int arity = 0;

    if (match(parser, TOKEN_RIGHT_PAREN)) return params;
    if (match(parser, TOKEN_DOT_DOT)) {
        Ast* param = parameter(parser, isLambda, "Expect variadic parameter name.");
        param->attribute.isVariadic = true;
        astAppendChild(params, param);
        if (match(parser, TOKEN_COMMA)) parseErrorAtPrevious(parser, "Cannot have more parameters following variadic parameter.");
        return params;
    }

    do {
        arity++;
        if (arity > UINT8_MAX) parseErrorAtCurrent(parser, "Can't have more than 255 parameters.");
        Ast* param = parameter(parser, isLambda, "Expect parameter name");
        astAppendChild(params, param);
    } while (match(parser, TOKEN_COMMA));
    return params;
}

static Ast* typeParameters(Parser* parser, Token token) {
    Ast* typeParams = emptyAst(AST_LIST_VAR, token);
    consume(parser, TOKEN_LESS, "Expect '<' before type parameters.");
    int count = 0;

    do {
        count++;
        if (count > UINT4_MAX) parseErrorAtCurrent(parser, "Can't have more than 15 type parameters.");
        consume(parser, TOKEN_IDENTIFIER, "Expect type parameter name.");
        Ast* typeParam = emptyAst(AST_EXPR_TYPE, previousToken(parser));
        astAppendChild(typeParams, typeParam);
    } while (match(parser, TOKEN_COMMA));

    consume(parser, TOKEN_GREATER, "Expect '>' after type parameters.");
    return typeParams;
}

static Ast* functionParameters(Parser* parser) {
    Token token = previousToken(parser);
    consume(parser, TOKEN_LEFT_PAREN, "Expect '(' after function keyword/name.");
    Ast* params = check(parser, TOKEN_RIGHT_PAREN) ? emptyAst(AST_LIST_VAR, token) : parameterList(parser, false, token);
    consume(parser, TOKEN_RIGHT_PAREN, "Expect ')' after parameters.");
    consume(parser, TOKEN_LEFT_BRACE, "Expect '{' before function body.");
    return params;
}

static Ast* lambdaParameters(Parser* parser) {
    Token token = previousToken(parser);
    if (!match(parser, TOKEN_PIPE)) return emptyAst(AST_LIST_VAR, token);
    Ast* params = parameterList(parser, true, token);
    consume(parser, TOKEN_PIPE, "Expect '|' after lambda parameters.");
    return params;
}

static Ast* function(Parser* parser, Ast* returnType, bool isAsync, bool isLambda, bool isVoid) {
    Token token = previousToken(parser);
    Ast* params = isLambda ? lambdaParameters(parser) : functionParameters(parser);
    Ast* body = block(parser);
    Ast* func = newAst(AST_EXPR_FUNCTION, token, 3, returnType, params, body);
    func->attribute.isAsync = isAsync;
    func->attribute.isLambda = isLambda;
    func->attribute.isVoid = isVoid;
    return func;
}

static bool isFieldDeclaration(Parser* parser) {
    if (check(parser, TOKEN_VAL) || check(parser, TOKEN_VAR)) return true;
    else if (check(parser, TOKEN_CLASS) && (checkNext(parser, TOKEN_VAL) || checkNext(parser, TOKEN_VAR))) return true;
    return false;
}

static Ast* fields(Parser* parser, Token* name) {
    Ast* fieldList = emptyAst(AST_LIST_FIELD, *name);

    while (isFieldDeclaration(parser)) {
        bool hasFieldType = false, hasInitializer = false;
        bool isClass = match(parser, TOKEN_CLASS);
        bool isMutable = (currentTokenType(parser) == TOKEN_VAR);
        advance(parser);

        Ast* fieldType = NULL;
        if (checkBoth(parser, TOKEN_IDENTIFIER)) {
            hasFieldType = true;
            fieldType = behaviorType(parser, "Expect field type.");
        }
        else if (checkEither(parser, TOKEN_IDENTIFIER, TOKEN_VOID) && checkNext(parser, TOKEN_FUN)) {
            hasFieldType = true;
            fieldType = callableType(parser, "Expect callable field type");
        }
        else if (check(parser, TOKEN_IDENTIFIER) && checkNext(parser, TOKEN_CLASS)) {
            hasFieldType = true;
            fieldType = metaclassType(parser, "Expect metaclass field type.");
        }

        Token fieldName = identifierToken(parser, "Expect field name.");
        Ast* initializer = NULL;
        if (match(parser, TOKEN_EQUAL)) {
            hasInitializer = true;
            initializer = expression(parser);
        }

        Ast* field = emptyAst(AST_DECL_FIELD, fieldName);       
        if (hasFieldType) astAppendChild(field, fieldType);
        if (hasInitializer) astAppendChild(field, initializer);

        field->attribute.isClass = isClass;
        field->attribute.isMutable = isMutable;
        field->attribute.isTyped = hasFieldType;
        astAppendChild(fieldList, field);
        consumerTerminator(parser, "Expect semicolon or new-line after field declaration.");
    }

    return fieldList;
}

static Ast* methods(Parser* parser, Token* name) {
    Ast* methodList = emptyAst(AST_LIST_METHOD, *name);

    while (!check(parser, TOKEN_RIGHT_BRACE) && !check(parser, TOKEN_EOF)) {
        bool isAsync = false, isClass = false, isInitializer = false, isVoid = false, hasReturnType = false;
        Ast* returnType = NULL;
        if (match(parser, TOKEN_ASYNC)) isAsync = true;
        if (match(parser, TOKEN_CLASS)) isClass = true;
        if (match(parser, TOKEN_VOID)) isVoid = true;

        if (checkBoth(parser, TOKEN_IDENTIFIER) || (check(parser, TOKEN_IDENTIFIER) && tokenIsOperator(nextToken(parser)))) {
            hasReturnType = true;
            returnType = behaviorType(parser, "Expect method return type.");
        }
        else if (checkEither(parser, TOKEN_IDENTIFIER, TOKEN_VOID) && checkNext(parser, TOKEN_FUN)) {
            hasReturnType = true;
            returnType = callableType(parser, "Expect method return type.");
        }
        else if (check(parser, TOKEN_IDENTIFIER) && checkNext(parser, TOKEN_CLASS)) {
            hasReturnType = true;
            returnType = metaclassType(parser, "Expect method return type.");
        }
        else returnType = emptyAst(AST_EXPR_TYPE, emptyToken());

        Token methodName = identifierToken(parser, "Expect method name.");
        if (previousToken(parser).length == 8 && memcmp(previousToken(parser).start, "__init__", 8) == 0) {
            isInitializer = true;
        }

        Ast* typeParams = check(parser, TOKEN_LESS) ? typeParameters(parser, methodName) : NULL;
        Ast* methodParams = functionParameters(parser);
        Ast* methodBody = block(parser);
        Ast* method = newAst(AST_DECL_METHOD, methodName, 3, returnType, methodParams, methodBody);
        if (typeParams != NULL) {
            method->attribute.isGeneric = true;
            astAppendChild(method, typeParams);
        }

        method->attribute.isAsync = isAsync;
        method->attribute.isClass = isClass;
        method->attribute.isInitializer = isInitializer;
        method->attribute.isVoid = isVoid;
        astAppendChild(methodList, method);
    }

    return methodList;
}

static Ast* superclass_(Parser* parser) {
    if (match(parser, TOKEN_EXTENDS)) {
        return identifier(parser, "Expect super class name.");
    }
    return emptyAst(AST_EXPR_VARIABLE, parser->rootClass);
}

static Ast* traits(Parser* parser, Token* name) {
    Ast* traitList = emptyAst(AST_LIST_VAR, *name);
    if (!match(parser, TOKEN_WITH)) return traitList;
    uint8_t traitCount = 0;

    do {
        traitCount++;
        if (traitCount > UINT4_MAX) {
            parseErrorAtCurrent(parser, "Can't have more than 15 parameters.");
        }

        Ast* trait = identifier(parser, "Expect trait name.");
        astAppendChild(traitList, trait);
    } while (match(parser, TOKEN_COMMA));

    return traitList;
}

static Ast* class_(Parser* parser, Token token, bool canAssign) {
    Token className = syntheticToken("@");
    Ast* superClass = superclass_(parser);
    Ast* traitList = traits(parser, &className);
    consume(parser, TOKEN_LEFT_BRACE, "Expect '{' before class body.");
    Ast* fieldList = fields(parser, &className);
    Ast* methodList = methods(parser, &className);
    consume(parser, TOKEN_RIGHT_BRACE, "Expect '}' after class body.");
    return newAst(AST_EXPR_CLASS, className, 4, superClass, traitList, fieldList, methodList);
}

static Ast* trait(Parser* parser, Token token, bool canAssign) {
    Token traitName = syntheticToken("@");
    Ast* traitList = traits(parser, &traitName);
    consume(parser, TOKEN_LEFT_BRACE, "Expect '{' before trait body.");
    Ast* methodList = methods(parser, &traitName);
    consume(parser, TOKEN_RIGHT_BRACE, "Expect '}' after trait body.");
    return newAst(AST_EXPR_TRAIT, traitName, 2, traitList, methodList);
}

static Ast* super_(Parser* parser, Token token, bool canAssign) {
    consume(parser, TOKEN_DOT, "Expect '.' after 'super'.");
    consume(parser, TOKEN_IDENTIFIER, "Expect superclass method name.");
    Token method = previousToken(parser);

    if (match(parser, TOKEN_LEFT_PAREN)) {
        Ast* arguments = argumentList(parser);
        return newAst(AST_EXPR_SUPER_INVOKE, method, 1, arguments);
    }
    else return emptyAst(AST_EXPR_SUPER_GET, method);
}

static Ast* this_(Parser* parser, Token token, bool canAssign) { 
    return emptyAst(AST_EXPR_THIS, token);
}

static Ast* unary(Parser* parser, Token token, bool canAssign) {
    Ast* expr = parsePrecedence(parser, PREC_UNARY);
    return newAst(AST_EXPR_UNARY, token, 1, expr);
}

static Ast* yield(Parser* parser, Token token, bool canAssign) {
    if (match(parser, TOKEN_RIGHT_PAREN) || match(parser, TOKEN_RIGHT_BRACKET) || match(parser, TOKEN_RIGHT_BRACE)
        || match(parser, TOKEN_COMMA) || match(parser, TOKEN_SEMICOLON)) 
    {
        return emptyAst(AST_EXPR_YIELD, token);
    }

    bool isYieldFrom = match(parser, TOKEN_FROM);
    Ast* expr = expression(parser);
    Ast* ast = newAst(AST_EXPR_YIELD, token, 1, expr);
    ast->attribute.isYieldFrom = isYieldFrom;
    return ast;
}

static Ast* async(Parser* parser, Token token, bool canAssign) { 
    if (match(parser, TOKEN_FUN)) {
        return function(parser, emptyAst(AST_EXPR_TYPE, emptyToken()), true, false, false);
    }
    else if (match(parser, TOKEN_LEFT_BRACE)) {
        return function(parser, emptyAst(AST_EXPR_TYPE, emptyToken()), true, true, false);
    }
    else {
        parseErrorAtPrevious(parser, "Can only use 'async' as expression modifier for anonymous functions or lambda.");
        return NULL;
    }
}

static Ast* await(Parser* parser, Token token, bool canAssign) { 
    Ast* expr = expression(parser);
    return newAst(AST_EXPR_AWAIT, token, 1, expr);
}

ParseRule parseRules[] = {
    [TOKEN_LEFT_PAREN]     = {grouping,      call,        PREC_CALL,        true},
    [TOKEN_RIGHT_PAREN]    = {NULL,          NULL,        PREC_NONE,        false},
    [TOKEN_LEFT_BRACKET]   = {collection,    subscript,   PREC_CALL,        true},
    [TOKEN_RIGHT_BRACKET]  = {NULL,          NULL,        PREC_NONE,        false},
    [TOKEN_LEFT_BRACE]     = {lambda,        NULL,        PREC_NONE,        true},
    [TOKEN_RIGHT_BRACE]    = {NULL,          NULL,        PREC_NONE,        false},
    [TOKEN_COLON]          = {NULL,          NULL,        PREC_NONE,        false},
    [TOKEN_COMMA]          = {NULL,          NULL,        PREC_NONE,        false},
    [TOKEN_MINUS]          = {unary,         binary,      PREC_TERM,        true},
    [TOKEN_MODULO]         = {NULL,          binary,      PREC_FACTOR,      false},
    [TOKEN_PIPE]           = {NULL,          NULL,        PREC_NONE,        false},
    [TOKEN_PLUS]           = {NULL,          binary,      PREC_TERM,        false},
    [TOKEN_QUESTION]       = {NULL,          question,    PREC_CALL,        false},
    [TOKEN_SEMICOLON]      = {NULL,          NULL,        PREC_NONE,        true},
    [TOKEN_SLASH]          = {NULL,          binary,      PREC_FACTOR,      false},
    [TOKEN_STAR]           = {NULL,          binary,      PREC_FACTOR,      false},
    [TOKEN_BANG]           = {unary,         NULL,        PREC_NONE,        true},
    [TOKEN_BANG_EQUAL]     = {NULL,          binary,      PREC_EQUALITY,    false},   
    [TOKEN_EQUAL]          = {NULL,          NULL,        PREC_NONE,        false},
    [TOKEN_EQUAL_EQUAL]    = {NULL,          binary,      PREC_EQUALITY,    false},
    [TOKEN_GREATER]        = {NULL,          binary,      PREC_COMPARISON,  false},
    [TOKEN_GREATER_EQUAL]  = {NULL,          binary,      PREC_COMPARISON,  false},
    [TOKEN_LESS]           = {NULL,          binary,      PREC_COMPARISON,  false},
    [TOKEN_LESS_EQUAL]     = {NULL,          binary,      PREC_COMPARISON,  false},
    [TOKEN_DOT]            = {NULL,          dot,         PREC_CALL,        false},
    [TOKEN_DOT_DOT]        = {NULL,          binary,      PREC_CALL,        false},
    [TOKEN_IDENTIFIER]     = {variable,      NULL,        PREC_NONE,        true},
    [TOKEN_STRING]         = {string,        NULL,        PREC_NONE,        true},
    [TOKEN_INTERPOLATION]  = {interpolation, NULL,        PREC_NONE,        true},
    [TOKEN_NUMBER]         = {literal,       NULL,        PREC_NONE,        true},
    [TOKEN_INT]            = {literal,       NULL,        PREC_NONE,        true},
    [TOKEN_AND]            = {NULL,          and_,        PREC_AND,         false},
    [TOKEN_AS]             = {NULL,          NULL,        PREC_NONE,        false},
    [TOKEN_ASYNC]          = {async,         NULL,        PREC_NONE,        true},
    [TOKEN_AWAIT]          = {await,         NULL,        PREC_NONE,        true},
    [TOKEN_BREAK]          = {NULL,          NULL,        PREC_NONE,        false},
    [TOKEN_CASE]           = {NULL,          NULL,        PREC_NONE,        false},
    [TOKEN_CATCH]          = {NULL,          NULL,        PREC_NONE,        false},
    [TOKEN_CLASS]          = {class_,        NULL,        PREC_NONE,        true},
    [TOKEN_CONTINUE]       = {NULL,          NULL,        PREC_NONE,        false},
    [TOKEN_DEFAULT]        = {NULL,          NULL,        PREC_NONE,        false},
    [TOKEN_ELSE]           = {NULL,          NULL,        PREC_NONE,        false},
    [TOKEN_EXTENDS]        = {NULL,          NULL,        PREC_NONE,        false},
    [TOKEN_FALSE]          = {literal,       NULL,        PREC_NONE,        true},
    [TOKEN_FINALLY]        = {NULL,          NULL,        PREC_NONE,        false},
    [TOKEN_FOR]            = {NULL,          NULL,        PREC_NONE,        false},
    [TOKEN_FROM]           = {NULL,          NULL,        PREC_NONE,        false},
    [TOKEN_FUN]            = {closure,       NULL,        PREC_NONE,        true},
    [TOKEN_IF]             = {NULL,          NULL,        PREC_NONE,        false},
    [TOKEN_NAMESPACE]      = {NULL,          NULL,        PREC_NONE,        false},
    [TOKEN_NIL]            = {literal,       NULL,        PREC_NONE,        true},
    [TOKEN_OR]             = {NULL,          or_,         PREC_OR,          false},
    [TOKEN_REQUIRE]        = {NULL,          NULL,        PREC_NONE,        false},
    [TOKEN_RETURN]         = {NULL,          NULL,        PREC_NONE,        false},
    [TOKEN_SUPER]          = {super_,        NULL,        PREC_NONE,        true},
    [TOKEN_SWITCH]         = {NULL,          NULL,        PREC_NONE,        false},
    [TOKEN_THIS]           = {this_,         NULL,        PREC_NONE,        true},
    [TOKEN_THROW]          = {NULL,          NULL,        PREC_NONE,        false},
    [TOKEN_TRAIT]          = {trait,         NULL,        PREC_NONE,        true},
    [TOKEN_TRUE]           = {literal,       NULL,        PREC_NONE,        true},
    [TOKEN_TRY]            = {NULL,          NULL,        PREC_NONE,        false},
    [TOKEN_TYPE_]          = {NULL,          NULL,        PREC_NONE,        false},
    [TOKEN_USING]          = {NULL,          NULL,        PREC_NONE,        false},
    [TOKEN_VAL]            = {NULL,          NULL,        PREC_NONE,        false},
    [TOKEN_VAR]            = {NULL,          NULL,        PREC_NONE,        false},
    [TOKEN_VOID]           = {NULL,          NULL,        PREC_NONE,        false},
    [TOKEN_WHILE]          = {NULL,          NULL,        PREC_NONE,        false},
    [TOKEN_WITH]           = {NULL,          NULL,        PREC_NONE,        false},
    [TOKEN_YIELD]          = {yield,         NULL,        PREC_NONE,        true},
    [TOKEN_ERROR]          = {NULL,          NULL,        PREC_NONE,        false},
    [TOKEN_EMPTY]          = {NULL,          NULL,        PREC_NONE,        true},
    [TOKEN_NEW_LINE]       = {NULL,          NULL,        PREC_NONE,        true},
    [TOKEN_EOF]            = {NULL,          NULL,        PREC_NONE,        true},
};

static Ast* parsePrefix(Parser* parser, Precedence precedence, bool canAssign) {
    ParsePrefixFn prefixRule = getRule(previousTokenType(parser))->prefix;
    if (prefixRule == NULL) {
        parseErrorAtPrevious(parser, "Expect expression.");
        return NULL;
    }
    return prefixRule(parser, previousToken(parser), canAssign);
}

static Ast* parseInfix(Parser* parser, Precedence precedence, Ast* left, bool canAssign) {
    while (precedence <= getRule(parser->current.type)->precedence) {
        advance(parser);
        ParseInfixFn infixRule = getRule(previousTokenType(parser))->infix;
        left = infixRule(parser, previousToken(parser), left, canAssign);
    }

    if (!canAssign && match(parser, TOKEN_EQUAL)) {
        parseErrorAtPrevious(parser, "Invalid assignment target.");
    }
    return left;
}

static Ast* parsePrecedence(Parser* parser, Precedence precedence) {
    advance(parser);
    bool canAssign = precedence <= PREC_ASSIGNMENT;
    Ast* left = parsePrefix(parser, precedence, canAssign);
    return parseInfix(parser, precedence, left, canAssign);
}

static ParseRule* getRule(TokenSymbol type) {
    return &parseRules[type];
}

static Ast* expression(Parser* parser) {
    return parsePrecedence(parser, PREC_ASSIGNMENT);
}

static Ast* block(Parser* parser) {
    Token token = previousToken(parser);
    Ast* stmtList = emptyAst(AST_LIST_STMT, token);
    while (!check(parser, TOKEN_RIGHT_BRACE) && !check(parser, TOKEN_EOF)) {
        Ast* decl = declaration(parser);
        astAppendChild(stmtList, decl);
    }

    consume(parser, TOKEN_RIGHT_BRACE, "Expect '}' after block.");
    return newAst(AST_STMT_BLOCK, token, 1, stmtList);
}

static Ast* awaitStatement(Parser* parser) {
    Token token = previousToken(parser);
    Ast* expr = expression(parser);
    consumerTerminator(parser, "Expect semicolon or new line after awaiting promise.");
    return newAst(AST_STMT_AWAIT, token, 1, expr);
}

static Ast* breakStatement(Parser* parser) {
    Ast* stmt = emptyAst(AST_STMT_BREAK, previousToken(parser));
    consumerTerminator(parser, "Expect semicolon or new line after 'break'.");
    return stmt;
}

static Ast* continueStatement(Parser* parser) {
    Ast* stmt = emptyAst(AST_STMT_CONTINUE, previousToken(parser));
    consumerTerminator(parser, "Expect semicolon or new line after 'continue'.");
    return stmt;
}

static Ast* expressionStatement(Parser* parser) {
    Token token = previousToken(parser);
    Ast* expr = expression(parser);
    consumerTerminator(parser, "Expect semicolon or new line after expression.");
    return newAst(AST_STMT_EXPRESSION, token, 1, expr);
}

static Ast* forStatement(Parser* parser) {
    Token token = previousToken(parser);
    Ast* stmt = emptyAst(AST_STMT_FOR, token);
    consume(parser, TOKEN_LEFT_PAREN, "Expect '(' after 'for'.");

    if (!match(parser, TOKEN_VAL) && !match(parser, TOKEN_VAR)) {
        parseErrorAtCurrent(parser, "Expect 'val' or 'var' keyword after '(' in for loop.");
    }
    bool isMutable = (previousTokenType(parser) == TOKEN_VAR);
    Ast* decl = emptyAst(AST_LIST_VAR, previousToken(parser));

    if (match(parser, TOKEN_LEFT_PAREN)) { 
        Ast* key = identifier(parser, "Expect first variable name after '('.");
        key->attribute.isMutable = isMutable;
        astAppendChild(decl, key);
        consume(parser, TOKEN_COMMA, "Expect ',' after first variable declaration.");
        
        Ast* value = identifier(parser, "Expect second variable name after ','.");
        value->attribute.isMutable = isMutable;
        astAppendChild(decl, value);
        consume(parser, TOKEN_RIGHT_PAREN, "Expect ')' after second variable declaration.");
    }
    else {
        Ast* element = identifier(parser, "Expect variable name after 'var'.");
        element->attribute.isMutable = isMutable;
        astAppendChild(decl, element);
    }

    consume(parser, TOKEN_COLON, "Expect ':' after variable name.");
    Ast* expr = expression(parser);
    consume(parser, TOKEN_RIGHT_PAREN, "Expect ')' after loop expression.");
    Ast* body = statement(parser);
    return newAst(AST_STMT_FOR, token, 3, decl, expr, body);
}

static Ast* ifStatement(Parser* parser) {
    Token token = previousToken(parser);
    consume(parser, TOKEN_LEFT_PAREN, "Expect '(' after 'if'.");
    Ast* condition = expression(parser);
    consume(parser, TOKEN_RIGHT_PAREN, "Expect ')' after condition.");
    Ast* thenBranch = statement(parser);

    Ast* stmt = newAst(AST_STMT_IF, token, 2, condition, thenBranch);
    if (match(parser, TOKEN_ELSE)) {
        Ast* elseBranch = statement(parser);
        astAppendChild(stmt, elseBranch);
    }
    return stmt;
}

static Ast* requireStatement(Parser* parser) {
    Token token = previousToken(parser);
    Ast* expr = expression(parser);
    consumerTerminator(parser, "Expect semicolon or new line after required file path.");
    return newAst(AST_STMT_REQUIRE, token, 1, expr);
}

static Ast* returnStatement(Parser* parser) {
    Token token = previousToken(parser);
    if (match(parser, TOKEN_SEMICOLON) || !getRule(parser->current.type)->startExpr) {
        return emptyAst(AST_STMT_RETURN, token);
    }
    else {
        Ast* expr = expression(parser);
        consumerTerminator(parser, "Expect semicolon or new line after return value.");
        return newAst(AST_STMT_RETURN, token, 1, expr);
    }
}

static Ast* switchStatement(Parser* parser) {
    Ast* stmt = emptyAst(AST_STMT_SWITCH, previousToken(parser));
    consume(parser, TOKEN_LEFT_PAREN, "Expect '(' after 'switch'.");
    Ast* expr = expression(parser);
    astAppendChild(stmt, expr);

    consume(parser, TOKEN_RIGHT_PAREN, "Expect ')' after value.");
    consume(parser, TOKEN_LEFT_BRACE, "Expect '{' before switch cases.");
    Ast* caseListStmt = emptyAst(AST_LIST_STMT, previousToken(parser));
    astAppendChild(stmt, caseListStmt);

    int state = 0;
    int caseCount = 0;
    while (!match(parser, TOKEN_RIGHT_BRACE) && !check(parser, TOKEN_EOF)) {
        if (match(parser, TOKEN_CASE) || match(parser, TOKEN_DEFAULT)) {
            Token caseToken = previousToken(parser);
            if (state == 1) caseCount++;
            if (state == 2) parseErrorAtPrevious(parser, "Can't have another 'case' or 'default' after the default case.");

            if (caseToken.type == TOKEN_CASE) {
                state = 1;
                Ast* caseLabel = expression(parser);
                consume(parser, TOKEN_COLON, "Expect ':' after case value.");

                Token token = previousToken(parser);
                Ast* caseBody = statement(parser);
                Ast* caseStmt = newAst(AST_STMT_CASE, token, 2, caseLabel, caseBody);
                astAppendChild(caseListStmt, caseStmt);
            }
            else {
                state = 2;
                consume(parser, TOKEN_COLON, "Expect ':' after 'default'.");
                Ast* defaultBody = statement(parser);
                Ast* defaultStmt = newAst(AST_STMT_DEFAULT, syntheticToken("default"), 1, defaultBody);
                astAppendChild(stmt, defaultStmt);
            }
        }
    }

    return stmt;
}

static Ast* throwStatement(Parser* parser) {
    Token token = previousToken(parser);
    Ast* expr = expression(parser);
    consumerTerminator(parser, "Expect semicolon or new line after thrown exception.");
    return newAst(AST_STMT_THROW, token, 1, expr);
}

static Ast* tryStatement(Parser* parser) {
    Ast* stmt = emptyAst(AST_STMT_TRY, previousToken(parser));
    Ast* tryStmt = statement(parser);
    astAppendChild(stmt, tryStmt);

    if (match(parser, TOKEN_CATCH)) {
        consume(parser, TOKEN_LEFT_PAREN, "Expect '(' after 'catch'");
        consume(parser, TOKEN_IDENTIFIER, "Expect type name to catch");
        Token exceptionType = previousToken(parser);
        Ast* exceptionVar = identifier(parser, "Expect identifier after exception type.");

        consume(parser, TOKEN_RIGHT_PAREN, "Expect ')' after 'catch' statement");
        Ast* catchBody = statement(parser);
        Ast* catchStmt = newAst(AST_STMT_CATCH, exceptionType, 2, exceptionVar, catchBody);
        astAppendChild(stmt, catchStmt);
    }
    else {
        parseErrorAtCurrent(parser, "Must have a 'catch' statement following a 'try' statement.");
    }

    if (match(parser, TOKEN_FINALLY)) {
        Ast* finallyBody = statement(parser);
        Ast* finallyStmt = newAst(AST_STMT_FINALLY, syntheticToken("finally"), 1, finallyBody);
        astAppendChild(stmt, finallyStmt);
    }
    return stmt;
}

static Ast* usingStatement(Parser* parser) {
    Token token = previousToken(parser);
    Ast* stmt = emptyAst(AST_STMT_USING, token);
    Ast* _namespace = emptyAst(AST_LIST_VAR, token);
    Ast* subNamespace = NULL;
    Ast* alias = NULL;

    do {
        consume(parser, TOKEN_IDENTIFIER, "Expect namespace identifier.");
        subNamespace = emptyAst(AST_EXPR_VARIABLE, previousToken(parser));
        astAppendChild(_namespace, subNamespace);
    } while (match(parser, TOKEN_DOT));
    astAppendChild(stmt, _namespace);

    if (match(parser, TOKEN_AS)) {
        consume(parser, TOKEN_IDENTIFIER, "Expect alias after 'as'.");
        alias = emptyAst(AST_EXPR_VARIABLE, previousToken(parser));
        astAppendChild(stmt, alias);
    }

    consumerTerminator(parser, "Expect semicolon or new-line after using statement.");
    return stmt;
}

static Ast* whileStatement(Parser* parser) {
    Token token = previousToken(parser);
    consume(parser, TOKEN_LEFT_PAREN, "Expect '(' after 'while'.");
    Ast* condition = expression(parser);
    consume(parser, TOKEN_RIGHT_PAREN, "Expect ')' after loop condition.");
    Ast* body = statement(parser);
    return newAst(AST_STMT_WHILE, token, 2, condition, body);
}

static Ast* yieldStatement(Parser* parser) {
    Token token = previousToken(parser);
    bool isYieldFrom = match(parser, TOKEN_FROM);
    if (match(parser, TOKEN_SEMICOLON) || !getRule(parser->current.type)->startExpr) {
        return emptyAst(AST_STMT_YIELD, token);
    }

    Ast* expr = expression(parser);
    consumerTerminator(parser, "Expect semicolon or new line after yield value.");
    Ast* ast = newAst(AST_STMT_YIELD, token, 1, expr);
    ast->attribute.isYieldFrom = isYieldFrom;
    return ast;
}

static Ast* statement(Parser* parser) {
    if (match(parser, TOKEN_AWAIT)) {
        return awaitStatement(parser);
    }
    else if (match(parser, TOKEN_BREAK)) {
        return breakStatement(parser);
    }
    else if (match(parser, TOKEN_CONTINUE)) {
        return continueStatement(parser);
    }
    else if (match(parser, TOKEN_FOR)) {
        return forStatement(parser);
    }
    else if (match(parser, TOKEN_IF)) {
        return ifStatement(parser);
    }
    else if (match(parser, TOKEN_REQUIRE)) {
        return requireStatement(parser);
    }
    else if (match(parser, TOKEN_RETURN)) {
        return returnStatement(parser);
    }
    else if (match(parser, TOKEN_SWITCH)) {
        return switchStatement(parser);
    }
    else if (match(parser, TOKEN_THROW)) {
        return throwStatement(parser);
    }
    else if (match(parser, TOKEN_TRY)) {
        return tryStatement(parser);
    }
    else if (match(parser, TOKEN_USING)) {
        return usingStatement(parser);
    }
    else if (match(parser, TOKEN_WHILE)) {
        return whileStatement(parser);
    }
    else if (match(parser, TOKEN_YIELD)) {
        return yieldStatement(parser);
    }
    else if (match(parser, TOKEN_LEFT_BRACE)) {
        return block(parser);
    }
    else {
        return expressionStatement(parser);
    }
}

static bool matchClassDeclaration(Parser* parser) {
    if (check(parser, TOKEN_CLASS) && checkNext(parser, TOKEN_IDENTIFIER)) {
        advance(parser);
        return true;
    }
    return false;
}

static Ast* classDeclaration(Parser* parser) {
    consume(parser, TOKEN_IDENTIFIER, "Expect class name.");
    Token name = previousToken(parser);
    Ast* typeParams = check(parser, TOKEN_LESS) ? typeParameters(parser, name) : NULL;
    Ast* superClass = superclass_(parser);
    Ast* traitList = traits(parser, &name);

    consume(parser, TOKEN_LEFT_BRACE, "Expect '{' before class body.");
    Ast* fieldList = fields(parser, &name);
    Ast* methodList = methods(parser, &name);
    consume(parser, TOKEN_RIGHT_BRACE, "Expect '}' after class body.");
    Ast* _class = newAst(AST_EXPR_CLASS, name, 4, superClass, traitList, fieldList, methodList);

    Ast* ast = newAst(AST_DECL_CLASS, name, 1, _class);
    if (typeParams != NULL) {
        ast->attribute.isGeneric = true;
        astAppendChild(ast, typeParams);
    }
    return ast;
}

static bool matchAsyncFunDeclaration(Parser* parser, bool* hasReturnType) {
    if (check(parser, TOKEN_FUN)) {
        advance(parser);
        *hasReturnType = false;
        return true;
    }
    else if (check(parser, TOKEN_VOID)) {
        *hasReturnType = false;
        return true;
    }
    else if (check(parser, TOKEN_IDENTIFIER)) {
        *hasReturnType = true;
        return true;
    }
    else return false;
}

static bool matchHigherOrderFunDeclaration(Parser* parser, bool* isAsync, bool* hasReturnType) {
    int index = parser->index;
    Token current = parser->current;
    advance(parser);
    advance(parser);

    do {
        advance(parser);
        if (currentTokenType(parser) != TOKEN_IDENTIFIER || nextTokenType(parser) == TOKEN_IDENTIFIER) {
            return resetIndex(parser, index, current, false);
        }
    } while (match(parser, TOKEN_COMMA));

    advance(parser);
    if (currentTokenType(parser) == TOKEN_RIGHT_PAREN && nextTokenType(parser) == TOKEN_LEFT_BRACKET) {
        return resetIndex(parser, index, current, false);
    }

    *hasReturnType = true;
    return resetIndex(parser, index, current, true);
}

static bool matchFunDeclaration(Parser* parser, bool* isAsync, bool* hasReturnType) {
    if (check(parser, TOKEN_ASYNC)) {
        advance(parser);
        *isAsync = true;
        return matchAsyncFunDeclaration(parser, hasReturnType);
    }
    else if (checkEither(parser, TOKEN_FUN, TOKEN_VOID) && checkNext(parser, TOKEN_IDENTIFIER)) {
        advance(parser);
        *isAsync = false;
        *hasReturnType = false;
        return true;
    }
    else if (checkBoth(parser, TOKEN_IDENTIFIER)) {
        *isAsync = false;
        *hasReturnType = true;
        return true;
    }
    else if (checkEither(parser, TOKEN_IDENTIFIER, TOKEN_VOID) && checkNext(parser, TOKEN_FUN)) {
        *isAsync = false;
        return matchHigherOrderFunDeclaration(parser, isAsync, hasReturnType);
    }
    else if (check(parser, TOKEN_IDENTIFIER) && checkNext(parser, TOKEN_CLASS)) {
        *isAsync = false;
        *hasReturnType = true;
        return true;
    }
    else return false;
}

static Ast* funDeclaration(Parser* parser, bool isAsync, bool hasReturnType) {
    bool isVoid = (previousTokenType(parser) == TOKEN_VOID);
    Ast* returnType = hasReturnType ? type_(parser, "Expect function return type.") : emptyAst(AST_EXPR_TYPE, emptyToken());
    consume(parser, TOKEN_IDENTIFIER, "Expect function name.");
    Token name = previousToken(parser);
    Ast* typeParams = check(parser, TOKEN_LESS) ? typeParameters(parser, name) : NULL;
    Ast* body = function(parser, returnType, isAsync, false, isVoid);

    Ast* ast = newAst(AST_DECL_FUN, name, 1, body);
    ast->attribute.isVoid = isVoid;
    if (typeParams != NULL) {
        ast->attribute.isGeneric = true;
        astAppendChild(ast, typeParams);
    }
    return ast;
}

static bool matchNamespaceDeclaration(Parser* parser) {
    return match(parser, TOKEN_NAMESPACE);
}

static Ast* namespaceDeclaration(Parser* parser) {
    Token token = previousToken(parser);
    Ast* _namespace = emptyAst(AST_LIST_VAR, token);
    uint8_t namespaceDepth = 0;

    do {
        if (namespaceDepth > UINT4_MAX) {
            parseErrorAtCurrent(parser, "Can't have more than 15 levels of namespace depth.");
        }
        astAppendChild(_namespace, identifier(parser, "Expect Namespace identifier."));
        namespaceDepth++;
    } while (match(parser, TOKEN_DOT));

    consumerTerminator(parser, "Expect semicolon or new line after namespace declaration.");
    return newAst(AST_DECL_NAMESPACE, token, 1, _namespace);
}

static bool matchTraitDeclaration(Parser* parser) {
    if (check(parser, TOKEN_TRAIT) && checkNext(parser, TOKEN_IDENTIFIER)) {
        advance(parser);
        return true;
    }
    return false;
}

static bool matchTypeDeclaration(Parser* parser) {
    if (check(parser, TOKEN_TYPE_) && checkNext(parser, TOKEN_IDENTIFIER)) {
        advance(parser);
        return true;
    }
    return false;
}

static Ast* traitDeclaration(Parser* parser) {
    consume(parser, TOKEN_IDENTIFIER, "Expect trait name.");
    Token name = previousToken(parser);
    Ast* typeParams = check(parser, TOKEN_LESS) ? typeParameters(parser, name) : NULL;
    Ast* traitList = traits(parser, &name);

    consume(parser, TOKEN_LEFT_BRACE, "Expect '{' before trait body.");
    Ast* methodList = methods(parser, &name);
    consume(parser, TOKEN_RIGHT_BRACE, "Expect '}' after trait body.");
    Ast* trait = newAst(AST_EXPR_TRAIT, name, 2, traitList, methodList);

    Ast* ast = newAst(AST_DECL_TRAIT, name, 1, trait);
    if (typeParams != NULL) {
        ast->attribute.isGeneric = true;
        astAppendChild(ast, typeParams);
    }
    return ast;
}

static Ast* typeDeclaration(Parser* parser) {
    consume(parser, TOKEN_IDENTIFIER, "Expect type name.");
    Token name = previousToken(parser);
    Ast* typeParams = check(parser, TOKEN_LESS) ? typeParameters(parser, name) : NULL;
    consume(parser, TOKEN_EQUAL, "Expect '=' after type name.");
    Ast* typeDef = type_(parser, "Expect type definition.");
    consumerTerminator(parser, "Expect semicolon or new line after type declaration.");

    Ast* ast = newAst(AST_DECL_TYPE, name, 1, typeDef);
    if (typeParams != NULL) {
        ast->attribute.isGeneric = true;
        astAppendChild(ast, typeParams);
    }
    return ast;
}

static bool matchVarDeclaration(Parser* parser, bool* isMutable) {
    if (match(parser, TOKEN_VAL)) {
        *isMutable = false;
        return true;
    }
    else if (match(parser, TOKEN_VAR)) {
        *isMutable = true;
        return true;
    }
    return false;
}

static Ast* varDeclaration(Parser* parser, bool isMutable) {
    consume(parser, TOKEN_IDENTIFIER, "Expect variable name.");
    Token identifier = previousToken(parser);
    Ast* varDecl = emptyAst(AST_DECL_VAR, identifier);
    varDecl->attribute.isMutable = isMutable;

    if (match(parser, TOKEN_EQUAL)) {
        Ast* expr = expression(parser);
        astAppendChild(varDecl, expr);
    }

    consumerTerminator(parser, "Expect semicolon or new line after variable declaration.");
    return varDecl;
}

static Ast* declaration(Parser* parser) {
    bool isAsync = false;
    bool hasReturnType = false;
    bool isMutable = false;

    if (matchClassDeclaration(parser)) {
        return classDeclaration(parser);
    }
    else if (matchFunDeclaration(parser, &isAsync, &hasReturnType)) {
        return funDeclaration(parser, isAsync, hasReturnType);
    }
    else if (matchNamespaceDeclaration(parser)) {
        return namespaceDeclaration(parser);
    }
    else if (matchTraitDeclaration(parser)) {
        return traitDeclaration(parser);
    }
    else if (matchTypeDeclaration(parser)) {
        return typeDeclaration(parser);
    }
    else if (matchVarDeclaration(parser, &isMutable)) {
        return varDeclaration(parser, isMutable);
    }
    else {
        return statement(parser);
    }
}

void initParser(Parser* parser, TokenStream* tokens, bool debugAst) {
    parser->tokens = tokens;
    parser->index = -1;
    parser->rootClass = syntheticToken("Object");
    parser->debugAst = debugAst;
    parser->hadError = false;
    parser->panicMode = false;
    advance(parser);
}

Ast* parse(Parser* parser) {
    Ast* ast = emptyAst(AST_KIND_NONE, emptyToken());
    while (!match(parser, TOKEN_EOF)) {
        if (setjmp(parser->jumpBuffer) == 0) {
            Ast* decl = declaration(parser);
            astAppendChild(ast, decl);
        }
        else synchronize(parser);
    }

    if (parser->debugAst) {
        astOutput(ast, 0);
        printf("\n");
    }
    return ast;
}
