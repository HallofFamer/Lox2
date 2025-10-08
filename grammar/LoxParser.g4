parser grammar Lox;
options { tokenVocab = LoxLexer; }

program: declaration* EOF;
declaration: classDecl | funDecl | namespaceDecl | traitDecl | varDecl | statement;
classDecl: CLASS IDENTIFIER classBody;
fieldDecl: CLASS? (VAL | VAR) type? IDENTIFIER (EQ expression)? TERMINATOR;
funDecl: (FUN | IDENTIFIER | VOID) function;
namespaceDecl: NAMESPACE IDENTIFIER (DOT IDENTIFIER)*;
traitDecl: TRAIT IDENTIFIER traitBody;
varDecl: (VAL | VAR) IDENTIFIER (EQ expression)? TERMINATOR;

statement: awaitStmt | breakStmt | caseStmt | continueStmt | exprStmt | forStmt | ifStmt | requireStmt | returnStmt | switchStmt | usingStmt | whileStmt | yieldStmt | block;
awaitStmt: AWAIT expression TERMINATOR;
breakStmt: BREAK TERMINATOR;
caseStmt: CASE expression COLON statement;
catchStmt: CATCH LPAREN IDENTIFIER IDENTIFIER? RPAREN statement
continueStmt: CONTINUE TERMINATOR;
exprStmt: expression TERMINATOR;
forStmt: FOR LPAREN (VAL | VAR) (IDENTIFIER | LPAREN IDENTIFIER COMMA IDENTIFIER RPAREN) COLON expression RPAREN statement;
ifStmt: IF LPAREN expression RPAREN statement (ELSE statement)?;
requireStmt: REQUIRE expression TERMINATOR;
returnStmt: RETURN expression? TERMINATOR;
switchStmt: SWITCH caseStmt* (DEFAULT COLON statement)? 
tryStmt: TRY statement catchStmt (FINALLY statement)?
usingStmt: USING IDENTIFIER (DOT IDENTIFIER)* (AS IDENTIFIER)?;
whileStmt: WHILE LPAREN expression RPAREN statement;
yieldStmt: YIELD FROM? expression TERMINATOR;
block: LBRACE declaration* RBRACE;

expression: assignment;
assignment: ((call DOT)? IDENTIFIER EQ assignment) | logicOr;
logicOr: (logicAND OR logicAND)*;
logicAND: (equality AND equality)*;
equality: (comparison (BangEQ | EQEQ) comparison)*;
comparison: (term (LT | LTEQ | GT | GTEQ) term)*;
term: (factor (PLUS | MINUS) factor)*;
factor: (unary (STAR | SLASH | MODULO) unary)*;
unary: ((BANG | MINUS) unary) | call;
call: primary (LPAREN arguments? RPAREN | LSQUARE expression QUESTION? RSQUARE | DOT IDENTIFIER)*;
primary: 'nil' | 'true' | 'false' | INT | FLOAT | STRING | IDENTIFIER | (LPAREN expression RPAREN) | (LSQUARE arguments? RSQUARE) | (expression DOTDOT expression) | (CLASS classBody) | (FUN functionBody) | (SUPER DOT IDENTIFIER) | (TRAIT traitBody);

classBody: (EXTENDS IDENTIFIER)? (WITH parameters)? LBRACE fieldDecl* function* RBRACE;
function: ASYNC? CLASS? type? IDENTIFIER functionBody;
functionBody: LPAREN parameters? RPAREN block;
traitBody: (WITH parameters)? LBRACE CLASS? function* RBRACE;
behaviorType: IDENTIFIER CLASS?;
callableType: (type | VOID)? fun LPAREN (type (COMMA type)*)? RPAREN;
genericType: IDENTIFIER (LT type (COMMA type)*)? GT;
type: behaviorType | callableType | genericType
parameters: (VAR? type? IDENTIFIER (VAR? COMMA type? IDENTIFIER)*) | (DOTDOT type? IDENTIFIER);
arguments: expression (COMMA expression)*;