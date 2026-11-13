#ifndef SIMBOLOS_H
#define SIMBOLOS_H

#include <stdio.h>
#include "ast.h"
#include "builtins.h"

/* simbolos.h - Tabla de simbolos por alcance (libro, 2.7)
 *
 * Cada alcance apunta al que lo contiene ("anterior") y se busca hacia
 * afuera. Cadena: predefinido > global > procedure o main > bloques.
 * Los campos de la maquina van aparte (solo con el punto). Los alcances
 * no se borran al salir porque el arbol apunta a sus entradas. */

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

    /* Valor en compilacion: const y campos desde el primer recorrido,
     * parametros e indices en el segundo, variables al generar. */
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

/* No verifica nada: antes se usa alcance_buscar_local */
Simbolo *alcance_insertar(Alcance *a, const char *nombre, ClaseSimbolo clase,
                          TipoDato tipo, Nodo *decl);

/* Busca solo en el alcance dado. Sirve para detectar redeclaraciones. */
Simbolo *alcance_buscar_local(const Alcance *a, const char *nombre);

/* Busca hacia afuera (bloque anidado mas cercano) */
Simbolo *alcance_buscar(const Alcance *a, const char *nombre);

/* Imprime todos los alcances con sus entradas (modo --tabla). */
void simbolos_imprimir(FILE *out);

/* Libera todos los alcances y sus entradas. */
void simbolos_liberar(void);

const char *simbolos_nombre_clase(ClaseSimbolo clase);

#endif
