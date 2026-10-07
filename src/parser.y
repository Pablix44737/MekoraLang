%{
/* parser.y - Analizador sintactico
 *
 * ESTADO: los tokens del lenguaje completo ya estan declarados (bison los
 * exporta a parser.tab.h, que es el header que incluye lexer.l). La gramatica
 * todavia reconoce solo "main { }"; las producciones se agregan en el paso 3.
 */

#include <stdio.h>
#include <stdlib.h>
#include "codegen.h"
#include "errores.h"

int  yylex(void);
void yyerror(const char *msg);

extern int   yylineno;
extern int   col_inicio;
extern FILE *salida;
%}

%define parse.error verbose

%union {
    double  numero;
    char   *texto;
}

/* --- Palabras reservadas --- */
%token MACHINE PROCEDURE MAIN CONST VAR
%token IF ELSE REPEAT AS
%token TIPO_NUMBER TIPO_BOOL
%token TRUE FALSE
%token AND OR NOT

/* --- Ejes y parametros (reservados, en mayuscula en el fuente) --- */
%token EJE_X EJE_Y EJE_Z EJE_E EJE_F EJE_S

/* --- Literales e identificadores --- */
%token <numero> NUMERO
%token <texto>  ID
%token <texto>  CADENA

/* --- Operadores aritmeticos --- */
%token MAS MENOS POR DIV MOD

/* --- Operadores de comparacion --- */
%token IGUAL_IGUAL DISTINTO MENOR MAYOR MENOR_IGUAL MAYOR_IGUAL

/* --- Simbolos --- */
%token ASIGNA PUNTO COMA PUNTO_COMA DOS_PUNTOS
%token PAREN_ABRE PAREN_CIERRA LLAVE_ABRE LLAVE_CIERRA

%start programa

%%

programa
    : MAIN LLAVE_ABRE cuerpo_main LLAVE_CIERRA
      {
          emitir_prologo(salida);
          /* Aca va el codigo generado por las sentencias del cuerpo. */
          emitir_epilogo(salida);
      }
    ;

cuerpo_main
    : /* vacio */
    ;

%%

void yyerror(const char *msg)
{
    error_sintactico(yylineno, col_inicio, "%s", msg);
}
