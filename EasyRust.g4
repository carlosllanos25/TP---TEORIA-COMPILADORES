grammar EasyRust;

// Punto de entrada
program: statement+ EOF;

// Tipos de declaraciones
statement
    : functionDecl
    | printStmt
    | forLoop
    | whileLoop
    | variableDecl
    | assignment
    | exprStmt
    | ifStmt              
    ;

// Declaración de variable con tipo
variableDecl
    : 'let' IDENTIFIER ':' type ('=' expr)? ';'
    ;

// Declaración de función
functionDecl
    : 'f' IDENTIFIER '(' parameters? ')' ':' type '{' statement* '}'
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

// While loop statement (New Rule)
whileLoop
    : 'while' condition '{' statement* '}'
    ;

// Asignación de variable
assignment
    : IDENTIFIER '=' expr ';'
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
    : expr op=('*'|'/') expr             # MulDiv
    | expr op=('+'|'-') expr             # AddSub
    | '(' expr ')'                       # Parens
    | NUMBER                             # Number
    | BOOLEAN                            # Boolean
    | STRING                             # String
    | IDENTIFIER                         # Identifier
    | functionCall                       # CallFunction
    | '-' expr                           # Negate
    | '!' expr                           # Not
    | expr '?' expr ':' expr             # TernaryOp // Expresión condicional ternaria
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
    : expr comparisonOp expr
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

// Tokens
IDENTIFIER: [a-zA-Z_][a-zA-Z0-9_]*;
NUMBER: ([0-9]+ ('.' [0-9]*)? | '.' [0-9]+) ([eE] ('+'|'-')? [0-9]+)?;
BOOLEAN: 'true' | 'false';
STRING: '"' (~["\r\n])* '"';
WS: [ \t\r\n]+ -> skip;
COMMENT: '//' ~[\r\n]* -> skip;
ML_COMMENT: '/*' .*? '*/' -> skip;
