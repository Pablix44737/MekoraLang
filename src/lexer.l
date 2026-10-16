%option noyywrap
%option yylineno
%option nounput noinput

%{
/* lexer.l - Analizador lexico
 *
 * Implementa la especificacion lexica, seccion 3.
 *
 * Nota sobre las palabras reservadas: se reconocen con reglas explicitas
 * ubicadas ANTES de la regla {ID}. Flex aplica el criterio de coincidencia
 * mas larga y, ante empate, la regla que aparece primero en el archivo.
 * Por eso el orden de este archivo no es cosmetico.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "parser.tab.h"
#include "errores.h"

/* Seguimiento de columna. yylineno lo lleva flex solo; la columna hay que
 * contarla a mano. col_inicio guarda la columna donde arranca el token
 * actual, que es la que se reporta en los errores.
 *
 * Ademas se copia la posicion a yylloc, que es por donde bison la recibe.
 * Con eso cada regla de la gramatica puede preguntar donde empieza cada
 * simbolo (@1, @2...) y guardarlo en el nodo del arbol. */
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
    /* Van en mayuscula y las palabras reservadas en minuscula, asi que no
     * colisionan: "var x : number" es valido, "X" como identificador no. */

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
    /* YY_USER_ACTION no corre en el fin de archivo, asi que yylloc quedaria
     * apuntando al ultimo espacio o salto de linea leido. Se fija a mano
     * para que un error del tipo "falta una llave" marque donde termina
     * el archivo. */

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
