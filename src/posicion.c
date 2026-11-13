#include <stdio.h>
#include <string.h>
#include "posicion.h"
#include "recorrido.h"
#include "simbolos.h"
#include "builtins.h"
#include "evaluador.h"
#include "errores.h"

/* posicion.c - Segundo recorrido del analisis semantico
 *
 * Posicion conocida: un booleano por eje. AutoHome los pone en 1 y
 * DisableMotors en 0. En un if se unen las ramas: conocido solo si lo es
 * en las dos (no se mira la condicion). Las llamadas se recorren como
 * expansion en linea y cada repeat vuelta por vuelta.
 * Limites y feedrate: error si es constante, advertencia si no. */

typedef struct {
    int         conocida;
    const Nodo *causa;    /* el DisableMotors que la perdio, o NULL si nunca
                             hubo AutoHome                                 */
    const Nodo *si;       /* el if en cuya union se perdio, o NULL        */
} EstadoEje;

typedef struct {
    EstadoEje eje[3];     /* X, Y, Z */
} Estado;

static const char NOMBRE_EJE[3] = { 'X', 'Y', 'Z' };

static void ejecutar(Nodo *s, Estado *e);

/* --- Posicion conocida --- */

static void describir_causa(const EstadoEje *ej, char *buf, size_t tam)
{
    if (ej->si && ej->causa)
        snprintf(buf, tam, "despues del if de la linea %d la posicion solo es "
                 "conocida en una de las ramas (la perdio DisableMotors() en "
                 "la linea %d)", ej->si->linea, ej->causa->linea);
    else if (ej->si)
        snprintf(buf, tam, "despues del if de la linea %d la posicion solo es "
                 "conocida en una de las ramas", ej->si->linea);
    else if (ej->causa)
        snprintf(buf, tam, "DisableMotors() en la linea %d libero los motores "
                 "y no hubo AutoHome() despues", ej->causa->linea);
    else
        snprintf(buf, tam, "falta un AutoHome() antes");
}

/* union del if: conocido solo si lo es en las dos ramas */
static void unir(Estado *salida, const Estado *entonces, const Estado *sino,
                 const Nodo *si)
{
    int i;

    for (i = 0; i < 3; i++) {
        const EstadoEje *a = &entonces->eje[i];
        const EstadoEje *b = &sino->eje[i];

        if (a->conocida && b->conocida) {
            salida->eje[i] = *a;
        } else if (!a->conocida && !b->conocida) {
            salida->eje[i] = *a;   /* se perdio en las dos: vale la primera */
        } else {
            salida->eje[i] = a->conocida ? *b : *a;
            salida->eje[i].si = si;
        }
    }
}

/* --- Builtins --- */

/* todo sale de la fila del builtin */
static void ejecutar_builtin(Nodo *s, const Builtin *b, Estado *e)
{
    Nodo *arg;
    int desconocido[3] = { 0, 0, 0 };
    int primero = -1;
    int i;

    /* Posicion conocida: Move y Draw sobre un eje desconocido. */
    if (b->efecto == EF_MUEVE) {
        for (arg = s->llamada.args; arg; arg = arg->sig) {
            if (arg->tipo != N_ARG_EJE)
                continue;
            i = recorrido_indice_eje(arg->arg_eje.eje);
            if (i >= 0 && !e->eje[i].conocida) {
                desconocido[i] = 1;
                if (primero < 0)
                    primero = i;
            }
        }
        if (primero >= 0) {
            char ejes[16] = "";
            char causa[256];
            int cuantos = 0;

            for (i = 0; i < 3; i++)
                if (desconocido[i]) {
                    size_t l = strlen(ejes);
                    snprintf(ejes + l, sizeof ejes - l, "%s%c",
                             cuantos++ ? ", " : "", NOMBRE_EJE[i]);
                }
            describir_causa(&e->eje[primero], causa, sizeof causa);
            recorrido_error(s, "'%s' con posicion desconocida en %s %s: %s",
                            b->nombre, cuantos > 1 ? "los ejes" : "el eje",
                            ejes, causa);
        }
    }

    /* Valores de los argumentos: limites, feedrate y division por cero. */
    for (arg = s->llamada.args; arg; arg = arg->sig) {
        const Nodo *valor = arg->tipo == N_ARG_EJE ? arg->arg_eje.valor : arg;
        const Nodo *culpable = NULL;
        ResultadoEval r;
        double v = 0;

        if (!valor || valor->tipo == N_CADENA)
            continue;
        r = recorrido_evaluar(valor, &v, &culpable);
        if (arg->tipo == N_ARG_EJE)
            recorrido_verificar(b, arg, r, v, culpable);
    }

    /* Efecto sobre la posicion. */
    switch (b->efecto) {
    case EF_HOMING:
        /* AutoHome() referencia los tres ejes; AutoHome(X, Y) solo esos. */
        for (i = 0; i < 3; i++) {
            int nombrado = s->llamada.args == NULL;
            for (arg = s->llamada.args; arg; arg = arg->sig)
                if (arg->tipo == N_ARG_EJE &&
                    recorrido_indice_eje(arg->arg_eje.eje) == i)
                    nombrado = 1;
            if (nombrado) {
                e->eje[i].conocida = 1;
                e->eje[i].causa    = NULL;
                e->eje[i].si       = NULL;
            }
        }
        break;
    case EF_LIBERA:
        for (i = 0; i < 3; i++) {
            e->eje[i].conocida = 0;
            e->eje[i].causa    = s;
            e->eje[i].si       = NULL;
        }
        break;
    case EF_MUEVE:
    case EF_NINGUNO:
        break;
    }
}

/* --- Sentencias --- */

static void ejecutar_repeat(Nodo *s, Estado *e)
{
    Simbolo *indice = s->repetir.indice ? s->simbolo : NULL;
    long vueltas, k;

    if (!recorrido_vueltas(s, &vueltas))
        return;

    /* una vez por vuelta */
    recorrido_entrar_repeat(indice);
    for (k = 0; k < vueltas; k++) {
        recorrido_vuelta(indice, k);
        ejecutar(s->repetir.cuerpo, e);
    }
    recorrido_salir_repeat(indice);
}

static void ejecutar(Nodo *s, Estado *e)
{
    const Nodo *culpable;
    double v;

    switch (s->tipo) {
    case N_BLOQUE:
        for (s = s->bloque.sentencias; s; s = s->sig)
            ejecutar(s, e);
        break;

    case N_VAR:
        recorrido_evaluar(s->decl.valor, &v, &culpable);
        break;

    case N_ASIGNACION:
        recorrido_evaluar(s->asignacion.valor, &v, &culpable);
        break;

    case N_IF: {
        /* cada rama parte del estado de antes; despues se unen */
        Estado entonces = *e, sino = *e;

        recorrido_evaluar(s->si.cond, &v, &culpable);
        ejecutar(s->si.entonces, &entonces);
        if (s->si.sino)
            ejecutar(s->si.sino, &sino);
        unir(e, &entonces, &sino, s);
        break;
    }

    case N_REPEAT:
        ejecutar_repeat(s, e);
        break;

    case N_LLAMADA:
        if (s->simbolo->clase == S_BUILTIN) {
            ejecutar_builtin(s, s->simbolo->builtin, e);
        } else if (s->simbolo->clase == S_PROC &&
                   recorrido_entrar_proc(s, s->simbolo)) {
            ejecutar(s->simbolo->decl->proc.cuerpo, e);
            recorrido_salir_proc(s->simbolo);
        }
        break;

    default:
        break;
    }
}

/* --- Entrada --- */

void analizar_posicion(Nodo *programa)
{
    Estado e;
    int i;

    if (!recorrido_iniciar(programa, error_semantico))
        return;   /* no pasa si el primer recorrido termino bien */

    /* al encender no se conoce la posicion */
    for (i = 0; i < 3; i++) {
        e.eje[i].conocida = 0;
        e.eje[i].causa    = NULL;
        e.eje[i].si       = NULL;
    }

    ejecutar(programa->programa.principal, &e);
}
