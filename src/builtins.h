#ifndef BUILTINS_H
#define BUILTINS_H

#include "ast.h"

/* builtins.h - Tabla de builtins
 *
 * Cada builtin es una fila de datos: su nombre, los parametros que admite
 * y como se traduce a G-code. El analisis semantico valida las llamadas
 * recorriendo la fila, sin un caso por builtin, y la generacion de codigo
 * (paso 5) va a armar la linea de G-code a partir de la misma fila.
 * Agregar un builtin cuesta una fila en builtins.c.
 * Ver especificacion, seccion 7. */

/* Formas que puede tener un parametro en la llamada. */
typedef enum {
    P_POSICIONAL,   /* Wait(2): una expresion, sin nombre            */
    P_NOMBRADO,     /* Move(X: 10): un eje seguido de una expresion  */
    P_EJE_SOLO,     /* AutoHome(X): el eje sin valor                 */
    P_CADENA        /* Message("hola"): un literal entre comillas    */
} ClaseParam;

/* Lo que el builtin le hace a la posicion conocida (seccion 8.1). */
typedef enum {
    EF_NINGUNO,     /* no depende de la posicion ni la cambia        */
    EF_MUEVE,       /* sus ejes X, Y, Z exigen posicion conocida      */
    EF_HOMING,      /* deja conocidos los ejes nombrados, o todos    */
    EF_LIBERA       /* deja desconocidos los tres ejes               */
} Efecto;

typedef struct {
    ClaseParam  clase;
    char        eje;          /* P_NOMBRADO y P_EJE_SOLO; 0 en los demas  */
    TipoDato    tipo;         /* P_POSICIONAL y P_NOMBRADO                */
    int         obligatorio;
    char        letra;        /* letra con la que sale en el G-code       */
} ParamBuiltin;

#define MAX_PARAMS_BUILTIN 5

typedef struct {
    const char   *nombre;
    int           n_params;
    ParamBuiltin  params[MAX_PARAMS_BUILTIN];

    int           requiere_posicion;   /* al menos uno de X, Y, Z        */
    int           texto_sin_comentario; /* la cadena no puede tener ';'  */
    Efecto        efecto;

    /* Plantilla de G-code. Es el comando base; a continuacion se agregan,
     * en el orden de la tabla, los parametros presentes: " <letra><valor>"
     * para los que tienen valor, " <letra>" para un eje solo y " <texto>"
     * para una cadena. Una plantilla vacia emite solo la cadena (Raw). Si
     * tiene varias lineas, separadas por \n, se emiten todas. */
    const char   *plantilla;

    /* Opcional: se agrega al error cuando se le pasa un eje que no admite.
     * Lo usa Move, para explicar por que no acepta E. */
    const char   *pista;
} Builtin;

/* Devuelve la fila del builtin con ese nombre, o NULL. */
const Builtin *builtin_buscar(const char *nombre);

/* Para recorrer la tabla entera (cargarla en la tabla de simbolos). */
extern const Builtin builtins[];
extern const int     n_builtins;

/* Parametro del builtin que corresponde a ese eje, o NULL si no lo admite. */
const ParamBuiltin *builtin_param_eje(const Builtin *b, char eje);

/* Cuantos parametros posicionales (P_POSICIONAL o P_CADENA) tiene. */
int builtin_n_posicionales(const Builtin *b);

/* El i-esimo parametro posicional, contando desde 0. */
const ParamBuiltin *builtin_posicional(const Builtin *b, int i);

#endif
