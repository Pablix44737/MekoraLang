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
 * Posicion conocida. Es un analisis de flujo de datos hacia adelante: el
 * estado son tres booleanos, uno por eje, que dicen si la posicion de ese
 * eje es conocida. Cada sentencia transforma el estado (AutoHome los pone
 * en verdadero, DisableMotors en falso) y en un if el estado de salida es
 * la union conservadora de las dos ramas: un eje queda conocido solo si lo
 * esta al final de las dos. El recorrido no mira el valor de la condicion,
 * asi que el resultado vale para cualquier valor que tome.
 *
 * Las llamadas se recorren como si el procedimiento estuviera escrito en
 * el lugar de la llamada: se entra con el estado de ese punto y se sale
 * con el que deja el cuerpo. Por eso Preparar puede hacer el AutoHome y
 * BarridoCama mover despues. Como no hay recursion, el recorrido termina.
 *
 * Limites. Cada argumento X, Y, Z de Move y Draw, y cada F, se evalua con
 * los valores de este punto del recorrido: las constantes, los indices de
 * repeat de la vuelta actual y los parametros de la llamada actual. Si es
 * constante y queda fuera de rango, es error; si depende de una variable,
 * es advertencia. La pila de llamadas, el ligado de parametros y esas
 * verificaciones estan en recorrido.c, porque la generacion de codigo
 * tambien las usa. */

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

/* ------------------------------------------------------------------ */
/* Posicion conocida                                                   */
/* ------------------------------------------------------------------ */

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

/* Union conservadora en el punto donde se juntan las dos ramas de un if:
 * un eje queda conocido solo si lo esta en las dos. */
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

/* ------------------------------------------------------------------ */
/* Builtins                                                            */
/* ------------------------------------------------------------------ */

/* Lo que hace el builtin sale de su fila en la tabla: el efecto sobre la
 * posicion y que parametros tiene. */
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

/* ------------------------------------------------------------------ */
/* Sentencias                                                          */
/* ------------------------------------------------------------------ */

static void ejecutar_repeat(Nodo *s, Estado *e)
{
    Simbolo *indice = s->repetir.indice ? s->simbolo : NULL;
    long vueltas, k;

    if (!recorrido_vueltas(s, &vueltas))
        return;

    /* El cuerpo se recorre una vez por vuelta, igual que el desenrollado. */
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
        /* Cada rama arranca con el estado de antes del if; si no hay else,
         * la rama vacia lo deja igual. Despues se unen. */
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

/* ------------------------------------------------------------------ */
/* Entrada                                                             */
/* ------------------------------------------------------------------ */

void analizar_posicion(Nodo *programa)
{
    Estado e;
    int i;

    if (!recorrido_iniciar(programa, error_semantico))
        return;   /* no pasa si el primer recorrido termino bien */

    /* Al encender, la impresora no sabe donde esta: los tres ejes
     * arrancan con posicion desconocida. */
    for (i = 0; i < 3; i++) {
        e.eje[i].conocida = 0;
        e.eje[i].causa    = NULL;
        e.eje[i].si       = NULL;
    }

    ejecutar(programa->programa.principal, &e);
}
