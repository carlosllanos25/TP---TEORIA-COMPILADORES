grammar EasyRust;

// Punto de entrada
program: statement+ EOF;

// Tipos de declaraciones
statement
    : functionDecl
    | printStmt
    | forLoop
    | whileLoop
    | matchStmt
    | variableDecl
    | assignment
    | exprStmt
    | ifStmt
    ;

// Declaración de variable con tipo
variableDecl
    : 'let' IDENTIFIER ':' (type | tupleType) '=' expr ';'
    ;

// Tipo de tupla
tupleType
    : '(' type (',' type)* ')'
    ;

// Declaración de función con retorno opcional
functionDecl
    : 'f' IDENTIFIER '(' parameters? ')' ':' type '{' statement* returnStmt? '}'
    ;

// Sentencia de retorno
returnStmt
    : 'return' expr ';'
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

// Expresión match simplificada
matchStmt
    : 'match' expr '{' matchArm+ '}'
    ;

// Brazo de match simplificado
matchArm
    : (expr | '_') '->' statement
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
    | expr comparisonOp expr             # Comparison
    | '(' expr (',' expr)* ')'           # Tuple
    | '(' expr ')'                       # Parens
    | IDENTIFIER '[' NUMBER ']'          # TupleAccess
    | NUMBER                             # Number
    | BOOLEAN                            # Boolean
    | STRING                             # String
    | IDENTIFIER                         # Identifier
    | functionCall                       # CallFunction
    | '-' expr                           # Negate
    | '!' expr                           # Not
    | expr '?' expr ':' expr             # TernaryOp
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
NUMBER: [0-9]+ ('.' [0-9]+)?;
BOOLEAN: 'true' | 'false';
STRING: '"' (~["\r\n])* '"';
WS: [ \t\r\n]+ -> skip;
COMMENT: '//' ~[\r\n]* -> skip;