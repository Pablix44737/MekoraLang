#include <string.h>
#include "builtins.h"

/* Abreviaturas para que cada fila entre en pocas lineas. */
#define NOMBRADO(e, oblig)  { P_NOMBRADO,   e, DATO_NUMBER, oblig, e }
#define EJE_SOLO(e)         { P_EJE_SOLO,   e, DATO_NUMBER, 0,     e }
#define POSICIONAL(letra)   { P_POSICIONAL, 0, DATO_NUMBER, 1,     letra }
#define CADENA              { P_CADENA,     0, DATO_NUMBER, 1,     0 }

/* Un builtin por operacion del dominio (seccion 1). Move no admite E:
 * extruir es otra operacion, y para eso estan Draw y Extrude. */
const Builtin builtins[] = {
    /* nombre, n, parametros, requiere_posicion, texto_sin_comentario,
     * efecto, plantilla, pista (opcional) */

    /* --- Movimiento y material --- */
    { "Move", 4,
      { NOMBRADO('X', 0), NOMBRADO('Y', 0), NOMBRADO('Z', 0), NOMBRADO('F', 0) },
      1, 0, EF_MUEVE, "G1",
      "Move solo desplaza el cabezal; para extruir estan Draw y Extrude" },
    { "Draw", 5,
      { NOMBRADO('X', 0), NOMBRADO('Y', 0), NOMBRADO('Z', 0), NOMBRADO('E', 1),
        NOMBRADO('F', 0) },
      1, 0, EF_MUEVE, "G1" },
    { "Extrude", 2,
      { NOMBRADO('E', 1), NOMBRADO('F', 0) },
      0, 0, EF_NINGUNO, "G1" },
    { "AutoHome", 3,
      { EJE_SOLO('X'), EJE_SOLO('Y'), EJE_SOLO('Z') },
      0, 0, EF_HOMING, "G28" },

    /* --- Temperatura --- */
    { "SetExtruderTemp",  1, { POSICIONAL('S') }, 0, 0, EF_NINGUNO, "M104" },
    { "WaitExtruderTemp", 1, { POSICIONAL('S') }, 0, 0, EF_NINGUNO, "M109" },
    { "SetBedTemp",       1, { POSICIONAL('S') }, 0, 0, EF_NINGUNO, "M140" },
    { "WaitBedTemp",      1, { POSICIONAL('S') }, 0, 0, EF_NINGUNO, "M190" },
    { "TurnOffHeaters",   0, { { 0 } },           0, 0, EF_NINGUNO, "M104 S0\nM140 S0" },

    /* --- Control y mantenimiento --- */
    { "Wait",          1, { POSICIONAL('S') },   0, 0, EF_NINGUNO, "G4" },
    { "Fan",           1, { NOMBRADO('S', 1) },  0, 0, EF_NINGUNO, "M106" },
    { "DisableMotors", 0, { { 0 } },             0, 0, EF_LIBERA,  "M84" },
    /* En G-code el ';' empieza un comentario: M117 cortaria el texto ahi. */
    { "Message",       1, { CADENA },            0, 1, EF_NINGUNO, "M117" },
    { "Raw",           1, { CADENA },            0, 0, EF_NINGUNO, "" },
};

const int n_builtins = sizeof builtins / sizeof builtins[0];

const Builtin *builtin_buscar(const char *nombre)
{
    int i;

    for (i = 0; i < n_builtins; i++)
        if (strcmp(builtins[i].nombre, nombre) == 0)
            return &builtins[i];
    return NULL;
}

const ParamBuiltin *builtin_param_eje(const Builtin *b, char eje)
{
    int i;

    for (i = 0; i < b->n_params; i++)
        if (b->params[i].eje == eje)
            return &b->params[i];
    return NULL;
}

static int es_posicional(const ParamBuiltin *p)
{
    return p->clase == P_POSICIONAL || p->clase == P_CADENA;
}

int builtin_n_posicionales(const Builtin *b)
{
    int i, n = 0;

    for (i = 0; i < b->n_params; i++)
        if (es_posicional(&b->params[i]))
            n++;
    return n;
}

const ParamBuiltin *builtin_posicional(const Builtin *b, int i)
{
    int j;

    for (j = 0; j < b->n_params; j++)
        if (es_posicional(&b->params[j]) && i-- == 0)
            return &b->params[j];
    return NULL;
}
