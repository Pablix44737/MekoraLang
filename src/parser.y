/* parser.y - Analizador sintactico y armado del arbol
 *
 * Precedencia por niveles, un no terminal por nivel, sin %left/%right.
 * Sin conflictos: llaves obligatorias en el if y ejes como tokens.
 */

%require "3.6"

/* ast.h antes de la union; "requires" lo copia en parser.tab.h */
%code requires {
#include "ast.h"
}

%code {
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "errores.h"

int  yylex(void);
void yyerror(const char *msg);

Nodo *arbol = NULL;

/* linea y columna de un simbolo: POS(@2) */
#define POS(loc) (loc).first_line, (loc).first_column
}

/* el lexer carga yylloc */
%locations

/* mensajes propios (yyreport_syntax_error) y LAC */
%define parse.error custom
%define parse.lac full

%union {
    double    numero;
    char     *texto;
    char      eje;
    TipoDato  tipo;
    Operador  op;
    Nodo     *nodo;
}

/* entre comillas: como aparece el token en los mensajes de error */

/* --- Palabras reservadas --- */
%token MACHINE      "'machine'"
%token PROCEDURE    "'procedure'"
%token MAIN         "'main'"
%token CONST        "'const'"
%token VAR          "'var'"
%token IF           "'if'"
%token ELSE         "'else'"
%token REPEAT       "'repeat'"
%token AS           "'as'"
%token TIPO_NUMBER  "'number'"
%token TIPO_BOOL    "'bool'"
%token TRUE         "'true'"
%token FALSE        "'false'"
%token AND          "'and'"
%token OR           "'or'"
%token NOT          "'not'"

/* --- Ejes y parametros (reservados, en mayuscula en el fuente) --- */
%token EJE_X "'X'"
%token EJE_Y "'Y'"
%token EJE_Z "'Z'"
%token EJE_E "'E'"
%token EJE_F "'F'"
%token EJE_S "'S'"

/* --- Literales e identificadores --- */
%token <numero> NUMERO  "numero"
%token <texto>  ID      "identificador"
%token <texto>  CADENA  "cadena"

/* --- Operadores aritmeticos --- */
%token MAS    "'+'"
%token MENOS  "'-'"
%token POR    "'*'"
%token DIV    "'/'"
%token MOD    "'%'"

/* --- Operadores de comparacion --- */
%token IGUAL_IGUAL  "'=='"
%token DISTINTO     "'!='"
%token MENOR        "'<'"
%token MAYOR        "'>'"
%token MENOR_IGUAL  "'<='"
%token MAYOR_IGUAL  "'>='"

/* --- Simbolos --- */
%token ASIGNA        "'='"
%token PUNTO         "'.'"
%token COMA          "','"
%token PUNTO_COMA    "';'"
%token DOS_PUNTOS    "':'"
%token PAREN_ABRE    "'('"
%token PAREN_CIERRA  "')'"
%token LLAVE_ABRE    "'{'"
%token LLAVE_CIERRA  "'}'"

%type <nodo> lista_decl declaracion machine_decl lista_campos campo
%type <nodo> const_decl procedure_decl params_opt lista_param param
%type <nodo> main_block bloque lista_sent sentencia
%type <nodo> var_decl asignacion if_stmt repeat_stmt llamada
%type <nodo> args_opt lista_arg arg
%type <nodo> expr or_expr and_expr not_expr rel_expr add_expr mul_expr
%type <nodo> unario primario
%type <tipo> tipo
%type <eje>  eje
%type <op>   relop

/* para liberar lo que se descarta al recuperarse de un error */
%destructor { free($$); }        <texto>
%destructor { ast_liberar($$); } <nodo>

%start programa

%%

/* --- 4.1 Estructura del programa --- */

programa
    : lista_decl main_block
        { arbol = ast_programa($1, $2); }
    ;

lista_decl
    : %empty                    { $$ = NULL; }
    | lista_decl declaracion    { $$ = ast_agregar($1, $2); }
    ;

declaracion
    : machine_decl
    | const_decl
    | procedure_decl
    ;

machine_decl
    : MACHINE ID LLAVE_ABRE lista_campos LLAVE_CIERRA
        { $$ = ast_maquina($2, $4, POS(@2)); }
    ;

lista_campos
    : campo                     { $$ = $1; }
    | lista_campos campo        { $$ = ast_agregar($1, $2); }
    ;

campo
    : ID DOS_PUNTOS expr PUNTO_COMA
        { $$ = ast_campo($1, $3, POS(@1)); }
    ;

const_decl
    : CONST ID DOS_PUNTOS tipo ASIGNA expr PUNTO_COMA
        { $$ = ast_const($2, $4, $6, POS(@2)); }
    ;

procedure_decl
    : PROCEDURE ID PAREN_ABRE params_opt PAREN_CIERRA bloque
        { $$ = ast_procedimiento($2, $4, $6, POS(@2)); }
    ;

params_opt
    : %empty                    { $$ = NULL; }
    | lista_param
    ;

lista_param
    : param                     { $$ = $1; }
    | lista_param COMA param    { $$ = ast_agregar($1, $3); }
    ;

param
    : ID DOS_PUNTOS tipo        { $$ = ast_param($1, $3, POS(@1)); }
    ;

tipo
    : TIPO_NUMBER               { $$ = DATO_NUMBER; }
    | TIPO_BOOL                 { $$ = DATO_BOOL; }
    | MACHINE                   { $$ = DATO_MACHINE; }
    ;

main_block
    : MAIN bloque               { $$ = $2; }
    ;

bloque
    : LLAVE_ABRE lista_sent LLAVE_CIERRA
        { $$ = ast_bloque($2, POS(@1)); }
    ;

lista_sent
    : %empty                    { $$ = NULL; }
    | lista_sent sentencia      { $$ = ast_agregar($1, $2); }
    ;

/* --- 4.2 Sentencias --- */

sentencia
    : var_decl
    | asignacion
    | if_stmt
    | repeat_stmt
    | llamada
    | bloque
    /* recuperacion: se descarta hasta el proximo ';' */
    | error PUNTO_COMA          { $$ = NULL; yyerrok; }
    ;

var_decl
    : VAR ID DOS_PUNTOS tipo ASIGNA expr PUNTO_COMA
        { $$ = ast_var($2, $4, $6, POS(@2)); }
    ;

asignacion
    : ID ASIGNA expr PUNTO_COMA
        { $$ = ast_asignacion($1, $3, POS(@1)); }
    ;

if_stmt
    : IF PAREN_ABRE expr PAREN_CIERRA bloque
        { $$ = ast_if($3, $5, NULL, POS(@1)); }
    | IF PAREN_ABRE expr PAREN_CIERRA bloque ELSE bloque
        { $$ = ast_if($3, $5, $7, POS(@1)); }
    | IF PAREN_ABRE expr PAREN_CIERRA bloque ELSE if_stmt
        { $$ = ast_if($3, $5, $7, POS(@1)); }
    ;

repeat_stmt
    : REPEAT expr bloque
        { $$ = ast_repeat($2, NULL, $3, POS(@1)); }
    | REPEAT expr AS ID bloque
        { $$ = ast_repeat($2, $4, $5, POS(@1)); }
    ;

llamada
    : ID PAREN_ABRE args_opt PAREN_CIERRA PUNTO_COMA
        { $$ = ast_llamada($1, $3, POS(@1)); }
    ;

args_opt
    : %empty                    { $$ = NULL; }
    | lista_arg
    ;

lista_arg
    : arg                       { $$ = $1; }
    | lista_arg COMA arg        { $$ = ast_agregar($1, $3); }
    ;

/* el eje solo es para AutoHome(X, Y) */
arg
    : expr
    | eje DOS_PUNTOS expr       { $$ = ast_arg_eje($1, $3, POS(@1)); }
    | eje                       { $$ = ast_arg_eje($1, NULL, POS(@1)); }
    | CADENA                    { $$ = ast_cadena($1, POS(@1)); }
    ;

eje
    : EJE_X                     { $$ = 'X'; }
    | EJE_Y                     { $$ = 'Y'; }
    | EJE_Z                     { $$ = 'Z'; }
    | EJE_E                     { $$ = 'E'; }
    | EJE_F                     { $$ = 'F'; }
    | EJE_S                     { $$ = 'S'; }
    ;

/* --- 4.3 Expresiones, de menor a mayor precedencia --- */
/* recursivas a izquierda: a - b - c es (a - b) - c */

expr
    : or_expr
    ;

or_expr
    : and_expr
    | or_expr OR and_expr
        { $$ = ast_binaria(OP_OR, $1, $3, POS(@2)); }
    ;

and_expr
    : not_expr
    | and_expr AND not_expr
        { $$ = ast_binaria(OP_AND, $1, $3, POS(@2)); }
    ;

not_expr
    : rel_expr
    | NOT rel_expr
        { $$ = ast_unaria(OP_NOT, $2, POS(@1)); }
    ;

/* No asociativo: a < b < c es un error de sintaxis. */
rel_expr
    : add_expr
    | add_expr relop add_expr
        { $$ = ast_binaria($2, $1, $3, POS(@2)); }
    ;

relop
    : IGUAL_IGUAL               { $$ = OP_IGUAL; }
    | DISTINTO                  { $$ = OP_DISTINTO; }
    | MENOR                     { $$ = OP_MENOR; }
    | MAYOR                     { $$ = OP_MAYOR; }
    | MENOR_IGUAL               { $$ = OP_MENOR_IGUAL; }
    | MAYOR_IGUAL               { $$ = OP_MAYOR_IGUAL; }
    ;

add_expr
    : mul_expr
    | add_expr MAS mul_expr
        { $$ = ast_binaria(OP_SUMA, $1, $3, POS(@2)); }
    | add_expr MENOS mul_expr
        { $$ = ast_binaria(OP_RESTA, $1, $3, POS(@2)); }
    ;

mul_expr
    : unario
    | mul_expr POR unario
        { $$ = ast_binaria(OP_MULT, $1, $3, POS(@2)); }
    | mul_expr DIV unario
        { $$ = ast_binaria(OP_DIV, $1, $3, POS(@2)); }
    | mul_expr MOD unario
        { $$ = ast_binaria(OP_MOD, $1, $3, POS(@2)); }
    ;

unario
    : primario
    | MENOS primario
        { $$ = ast_unaria(OP_NEG, $2, POS(@1)); }
    ;

primario
    : NUMERO                    { $$ = ast_numero($1, POS(@1)); }
    | TRUE                      { $$ = ast_bool(1, POS(@1)); }
    | FALSE                     { $$ = ast_bool(0, POS(@1)); }
    | ID                        { $$ = ast_id($1, POS(@1)); }
    | ID PUNTO ID               { $$ = ast_acceso($1, $3, POS(@1)); }
    | PAREN_ABRE expr PAREN_CIERRA
        { $$ = $2; }
    ;

%%

/* --- Mensajes de error de sintaxis --- */

extern char *yytext;

static const char *describir(yysymbol_kind_t simbolo)
{
    if (simbolo == YYSYMBOL_YYEOF)
        return "el fin del archivo";
    return yysymbol_name(simbolo);
}

/* Si es ID, numero o cadena se muestra el texto (yytext) */
static void describir_encontrado(yysymbol_kind_t simbolo, char *buf, size_t tam)
{
    switch (simbolo) {
    case YYSYMBOL_ID:
    case YYSYMBOL_NUMERO:
        snprintf(buf, tam, "'%.40s'", yytext);
        break;
    case YYSYMBOL_CADENA:
        snprintf(buf, tam, "%.40s", yytext);   /* ya trae sus comillas */
        break;
    default:
        snprintf(buf, tam, "%s", describir(simbolo));
        break;
    }
}

/* "se esperaba X y se encontro Y"; si son muchos, "no se esperaba Y" */
static int yyreport_syntax_error(const yypcontext_t *ctx)
{
    enum { MAX_ESPERADOS = 4 };
    yysymbol_kind_t esperados[MAX_ESPERADOS];
    const YYLTYPE  *pos = yypcontext_location(ctx);
    char encontrado[64];
    char mensaje[256];
    int  largo = 0;
    int  n, i;

    describir_encontrado(yypcontext_token(ctx), encontrado, sizeof encontrado);
    n = yypcontext_expected_tokens(ctx, esperados, MAX_ESPERADOS);

    if (n <= 0) {
        snprintf(mensaje, sizeof mensaje, "no se esperaba %s", encontrado);
    } else {
        largo += snprintf(mensaje + largo, sizeof mensaje - largo, "se esperaba ");
        for (i = 0; i < n; i++) {
            if (i > 0)
                largo += snprintf(mensaje + largo, sizeof mensaje - largo,
                                  i == n - 1 ? " o " : ", ");
            largo += snprintf(mensaje + largo, sizeof mensaje - largo,
                              "%s", describir(esperados[i]));
        }
        snprintf(mensaje + largo, sizeof mensaje - largo,
                 " y se encontro %s", encontrado);
    }

    error_sintactico(pos->first_line, pos->first_column, "%s", mensaje);
    return 0;
}

/* con parse.error custom solo se usa en casos como falta de memoria */
void yyerror(const char *msg)
{
    error_sintactico(yylloc.first_line, yylloc.first_column, "%s", msg);
}
