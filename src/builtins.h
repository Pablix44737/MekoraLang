#ifndef BUILTINS_H
#define BUILTINS_H

#include "ast.h"

/* builtins.h - Tabla de builtins
 *
 * Una fila por builtin: nombre, parametros y plantilla de G-code. La usan
 * el analisis semantico y el generador. */

/* Formas que puede tener un parametro en la llamada. */
typedef enum {
    P_POSICIONAL,   /* Wait(2): una expresion, sin nombre            */
    P_NOMBRADO,     /* Move(X: 10): un eje seguido de una expresion  */
    P_EJE_SOLO,     /* AutoHome(X): el eje sin valor                 */
    P_CADENA        /* Message("hola"): un literal entre comillas    */
} ClaseParam;

/* Efecto sobre la posicion conocida */
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

    /* Comando base; despues van los parametros presentes en el orden de la
     * tabla. Vacia: solo la cadena (Raw). Puede tener varias lineas. */
    const char   *plantilla;

    /* Texto extra para el error de eje no admitido (lo usa Move) */
    const char   *pista;
} Builtin;

/* Devuelve la fila del builtin con ese nombre, o NULL. */
const Builtin *builtin_buscar(const char *nombre);

/* La tabla completa */
extern const Builtin builtins[];
extern const int     n_builtins;

/* Parametro del builtin que corresponde a ese eje, o NULL si no lo admite. */
const ParamBuiltin *builtin_param_eje(const Builtin *b, char eje);

/* Cuantos parametros posicionales (P_POSICIONAL o P_CADENA) tiene. */
int builtin_n_posicionales(const Builtin *b);

/* El i-esimo parametro posicional, contando desde 0. */
const ParamBuiltin *builtin_posicional(const Builtin *b, int i);

#endif
