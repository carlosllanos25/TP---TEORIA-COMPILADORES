grammar EasyRust;

// Punto de entrada
program: statement+ EOF;

// Tipos de declaraciones
statement
    : variableDecl
    | assignmentStmt
    | functionDecl
    | printStmt
    | forLoop
    | whileLoop
    | ifStmt
    | exprStmt
    | returnStmt  // Añadido para reconocer declaraciones de retorno
    ;

// Declaración de variable con tipo
variableDecl
    : 'let' IDENTIFIER ':' type '=' expr ';'
    ;

// Declaración de función con retorno opcional
functionDecl
    : 'f' IDENTIFIER '(' parameters? ')' ':' type '{' statement* '}'
    ;

// Sentencia de retorno
returnStmt
    : 'return' expr ';'
    ;
// Asignación de variable
assignmentStmt
    : IDENTIFIER '=' expr ';'
    ;

// Parámetros de función con tipos
parameters
    : parameter (',' parameter)*
    ;

// Parámetro individual
parameter
    : IDENTIFIER ':' type
    ;

// Sentencia de impresión
printStmt
    : 'print' '(' expr ')' ';'
    ;

// Bucle for simplificado
forLoop
    : 'for' IDENTIFIER '=' expr ';' condition ';' IDENTIFIER '++' '{' statement+ '}'
    ;

// Bucle while
whileLoop
    : 'while' condition '{' statement+ '}'
    ;

// Expresión como sentencia
exprStmt
    : expr ';'
    ;

// Sentencia if
ifStmt
    : 'if' condition '{' statement+ '}' ('else' '{' statement+ '}')?
    ;

// Expresiones
expr
    : functionCall                       # CallFunction
    | IDENTIFIER                         # Identifier
    | '(' expr ')'                       # Parens
    | expr op=('*'|'/') expr             # MulDiv
    | expr op=('+'|'-') expr             # AddSub
    | NUMBER                             # Number
    | BOOLEAN                            # Boolean
    | STRING                             # String
    ;

// Llamada a función
functionCall
    : IDENTIFIER '(' arguments? ')'
    ;

// Argumentos de función
arguments
    : expr (',' expr)*
    ;

// Condición en bucle y en if
condition
    : '(' expr comparisonOp expr ')'
    ;

// Operadores de comparación
comparisonOp
    : '==' | '!=' | '<' | '>' | '<=' | '>='
    ;

// Tipos de datos
type
    : 'int'
    | 'float'
    | 'bool'
    | 'string'
    | 'void'
    | IDENTIFIER // Para tipos personalizados
    ;
ADD   : '+' ;
SUB   : '-' ;
MUL   : '*' ;
DIV   : '/' ;
// Tokens
IDENTIFIER: [a-zA-Z_][a-zA-Z0-9_]*;
NUMBER: [0-9]+ ('.' [0-9]+)?;
BOOLEAN: 'true' | 'false';
STRING: '"' (~["\r\n])* '"';
WS: [ \t\r\n]+ -> skip;
COMMENT: '//' ~[\r\n]* -> skip;