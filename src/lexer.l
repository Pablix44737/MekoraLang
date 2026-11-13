%option noyywrap
%option yylineno
%option nounput noinput

%{
/* lexer.l - Analizador lexico (spec seccion 3)
 *
 * Las palabras reservadas van ANTES de {ID}: ante un empate gana la
 * primera regla.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "parser.tab.h"
#include "errores.h"

/* La columna se cuenta a mano (la linea la lleva flex). col_inicio es
 * donde empieza el token. La posicion se copia a yylloc para bison. */
int columna    = 1;
int col_inicio = 1;

#define YY_USER_ACTION  {                                   \
        col_inicio = columna;                               \
        columna += yyleng;                                  \
        yylloc.first_line   = yylloc.last_line = yylineno;  \
        yylloc.first_column = col_inicio;                   \
        yylloc.last_column  = columna - 1;                  \
    }
%}

DIGITO      [0-9]
LETRA       [a-zA-Z_]
ID          {LETRA}({LETRA}|{DIGITO})*
NUMERO      {DIGITO}+("."{DIGITO}+)?
CADENA      \"[^\"\n]*\"

%%

    /* --- Elementos que se descartan --- */

\n                  { columna = 1; }
[ \t\r]+            { /* espacios */ }
"//"[^\n]*          { /* comentario de linea */ }

    /* --- Palabras reservadas (antes de {ID}) --- */

"machine"           { return MACHINE; }
"procedure"         { return PROCEDURE; }
"main"              { return MAIN; }
"const"             { return CONST; }
"var"               { return VAR; }
"if"                { return IF; }
"else"              { return ELSE; }
"repeat"            { return REPEAT; }
"as"                { return AS; }
"number"            { return TIPO_NUMBER; }
"bool"              { return TIPO_BOOL; }
"true"              { return TRUE; }
"false"             { return FALSE; }
"and"               { return AND; }
"or"                { return OR; }
"not"               { return NOT; }

    /* --- Ejes y parametros --- */
    /* en mayuscula: no chocan con las reservadas */

"X"                 { return EJE_X; }
"Y"                 { return EJE_Y; }
"Z"                 { return EJE_Z; }
"E"                 { return EJE_E; }
"F"                 { return EJE_F; }
"S"                 { return EJE_S; }

    /* --- Operadores de comparacion (los de dos caracteres primero) --- */

"=="                { return IGUAL_IGUAL; }
"!="                { return DISTINTO; }
"<="                { return MENOR_IGUAL; }
">="                { return MAYOR_IGUAL; }
"<"                 { return MENOR; }
">"                 { return MAYOR; }

    /* --- Operadores aritmeticos --- */

"+"                 { return MAS; }
"-"                 { return MENOS; }
"*"                 { return POR; }
"/"                 { return DIV; }
"%"                 { return MOD; }

    /* --- Simbolos --- */

"="                 { return ASIGNA; }
"."                 { return PUNTO; }
","                 { return COMA; }
";"                 { return PUNTO_COMA; }
":"                 { return DOS_PUNTOS; }
"("                 { return PAREN_ABRE; }
")"                 { return PAREN_CIERRA; }
"{"                 { return LLAVE_ABRE; }
"}"                 { return LLAVE_CIERRA; }

    /* --- Literales e identificadores --- */

{NUMERO}            {
                        yylval.numero = atof(yytext);
                        return NUMERO;
                    }

{CADENA}            {
                        /* Se guarda sin las comillas. */
                        size_t largo = strlen(yytext) - 2;
                        yylval.texto = malloc(largo + 1);
                        memcpy(yylval.texto, yytext + 1, largo);
                        yylval.texto[largo] = '\0';
                        return CADENA;
                    }

{ID}                {
                        yylval.texto = strdup(yytext);
                        return ID;
                    }

    /* --- Fin del archivo --- */
    /* posicion del fin de archivo, para errores como "falta una llave" */

<<EOF>>             {
                        yylloc.first_line   = yylloc.last_line = yylineno;
                        yylloc.first_column = yylloc.last_column = columna;
                        return 0;
                    }

    /* --- Errores --- */

\"[^\"\n]*$         {
                        error_lexico(yylineno, col_inicio,
                            "cadena sin cerrar");
                    }

.                   {
                        error_lexico(yylineno, col_inicio,
                            "caracter no reconocido '%c'", yytext[0]);
                    }

%%
