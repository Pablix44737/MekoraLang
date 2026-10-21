#ifndef SIMBOLOS_H
#define SIMBOLOS_H

#include <stdio.h>
#include "ast.h"
#include "builtins.h"

/* simbolos.h - Tabla de simbolos por alcance
 *
 * Es la tabla encadenada de la seccion 2.7 del libro del dragon: cada
 * alcance (Alcance) tiene sus propias entradas y un puntero al alcance que
 * lo contiene ("anterior", el "prev" del libro). Para resolver un nombre se
 * busca primero en el alcance actual y, si no esta, se sigue la cadena
 * hacia afuera. Asi se cumple la regla del bloque anidado mas cercano: si
 * un bloque interno declara un nombre que ya existe afuera, lo oculta.
 *
 * La cadena de alcances de un programa queda asi:
 *
 *     predefinido      los builtins (Move, Wait...)
 *       global         la maquina, las constantes y los procedimientos
 *         procedure P  los parametros y lo declarado en el cuerpo de P
 *           bloque     lo declarado en un { } interno, un if o un repeat
 *         main         lo declarado en el cuerpo de main
 *
 * Los campos de una maquina van en un alcance propio que no esta en la
 * cadena: solo se llega a ellos con el punto (Ender3.limite_x).
 *
 * A diferencia del ejemplo del libro, los alcances no se descartan al
 * salir del bloque: el arbol queda apuntando a las entradas, y el segundo
 * recorrido y la generacion de codigo las usan. Se liberan todos juntos al
 * final con simbolos_liberar().
 *
 * Cada alcance guarda sus entradas en una lista. El libro usa una tabla
 * hash por alcance; con programas de este tamano una lista alcanza. */

typedef enum {
    S_BUILTIN,      /* Move, Wait...                       */
    S_MAQUINA,      /* machine Ender3 { }                  */
    S_CAMPO,        /* limite_x, dentro de una maquina     */
    S_CONST,        /* const MARGEN : number = 10;         */
    S_PROC,         /* procedure Preparar(...)             */
    S_PARAM,        /* m: machine                          */
    S_VAR,          /* var vel : number = 1500;            */
    S_INDICE        /* el "as fila" de un repeat           */
} ClaseSimbolo;

typedef struct Simbolo Simbolo;
typedef struct Alcance Alcance;

struct Simbolo {
    const char   *nombre;    /* apunta al texto del arbol, no es propio */
    ClaseSimbolo  clase;
    TipoDato      tipo;
    Nodo         *decl;      /* nodo de la declaracion; NULL en un builtin */

    const Builtin *builtin;  /* S_BUILTIN: su fila en la tabla de builtins */
    Alcance       *campos;   /* S_MAQUINA: el alcance con sus campos       */

    /* Valor en compilacion. Las constantes y los campos lo tienen fijo
     * desde el primer recorrido. Los parametros y los indices de repeat
     * lo reciben en el segundo, en cada llamada o en cada vuelta, y lo
     * pierden al salir. Las variables nunca lo tienen. Los bool se
     * guardan como 0 o 1. */
    int     conocido;
    double  valor;

    Simbolo *sig;            /* siguiente entrada del mismo alcance */
};

struct Alcance {
    char      descripcion[64];   /* "global", "procedure Preparar"... */
    int       linea;             /* donde empieza, para --tabla       */
    int       nivel;             /* profundidad en la cadena          */
    Alcance  *anterior;          /* el que lo contiene, o NULL        */
    Simbolo  *simbolos;          /* en orden de declaracion           */
    Simbolo  *ultimo;
    Alcance  *sig_todos;         /* lista de todos, para liberar      */
};

/* Abre un alcance nuevo dentro de "anterior" (NULL para el de afuera). */
Alcance *alcance_abrir(Alcance *anterior, int linea, const char *fmt, ...);

/* Agrega una entrada al alcance. No verifica nada: quien llama ya miro
 * con alcance_buscar_local que el nombre no estuviera. */
Simbolo *alcance_insertar(Alcance *a, const char *nombre, ClaseSimbolo clase,
                          TipoDato tipo, Nodo *decl);

/* Busca solo en el alcance dado. Sirve para detectar redeclaraciones. */
Simbolo *alcance_buscar_local(const Alcance *a, const char *nombre);

/* Busca en el alcance dado y, si no esta, en los de afuera: la regla del
 * bloque anidado mas cercano. */
Simbolo *alcance_buscar(const Alcance *a, const char *nombre);

/* Imprime todos los alcances con sus entradas (modo --tabla). */
void simbolos_imprimir(FILE *out);

/* Libera todos los alcances y sus entradas. */
void simbolos_liberar(void);

const char *simbolos_nombre_clase(ClaseSimbolo clase);

#endif
