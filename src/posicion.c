#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "posicion.h"
#include "semantico.h"
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
 * es advertencia. */

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

/* Datos de la maquina. */
static const char *nombre_maquina;
static double      limite[3];
static double      feedrate_max;

/* Pila de llamadas activas, de main hacia adentro. Es el contexto que se
 * muestra en las notas de un error. */
static Nodo **pila;
static int    n_pila, cap_pila;

/* Indices de repeat activos, para decir su valor en los mensajes. */
static Simbolo **indices;
static int       n_indices, cap_indices;

/* Errores ya reportados. Un cuerpo de repeat se recorre muchas veces y un
 * procedimiento puede llamarse desde varios lugares: cada error se
 * reporta una vez por sentencia y por cadena de llamadas. */
typedef struct Reportado {
    const Nodo        *nodo;
    int                n;
    Nodo             **pila;
    struct Reportado  *sig;
} Reportado;

static Reportado *reportados;

static void ejecutar(Nodo *s, Estado *e);

/* ------------------------------------------------------------------ */
/* Utilidades                                                          */
/* ------------------------------------------------------------------ */

static void *agrandar(void *p, int *cap, size_t tam)
{
    *cap = *cap ? *cap * 2 : 8;
    p = realloc(p, *cap * tam);
    if (!p) {
        fprintf(stderr, "sin memoria en el analisis semantico\n");
        exit(2);
    }
    return p;
}

static int indice_eje(char eje)
{
    switch (eje) {
    case 'X': return 0;
    case 'Y': return 1;
    case 'Z': return 2;
    default:  return -1;
    }
}

/* Devuelve 1 si ese nodo ya se reporto con la misma cadena de llamadas, y
 * si no lo anota. */
static int ya_reportado(const Nodo *nodo)
{
    Reportado *r;

    for (r = reportados; r; r = r->sig)
        if (r->nodo == nodo && r->n == n_pila &&
            (n_pila == 0 || memcmp(r->pila, pila, n_pila * sizeof *pila) == 0))
            return 1;

    r = malloc(sizeof *r);
    if (!r) {
        fprintf(stderr, "sin memoria en el analisis semantico\n");
        exit(2);
    }
    r->nodo = nodo;
    r->n    = n_pila;
    r->pila = malloc((n_pila ? n_pila : 1) * sizeof *pila);
    if (!r->pila) {
        fprintf(stderr, "sin memoria en el analisis semantico\n");
        exit(2);
    }
    if (n_pila > 0)
        memcpy(r->pila, pila, n_pila * sizeof *pila);
    r->sig = reportados;
    reportados = r;
    return 0;
}

/* Una nota por cada llamada de la cadena, de la mas interna a main. */
static void notas_de_contexto(void)
{
    int i;

    for (i = n_pila - 1; i >= 0; i--)
        nota(pila[i]->linea, pila[i]->columna, "en la llamada a '%s'",
             pila[i]->llamada.nombre);
}

static void error_en(const Nodo *n, const char *fmt, ...)
{
    char msg[512];
    va_list args;

    if (ya_reportado(n))
        return;
    va_start(args, fmt);
    vsnprintf(msg, sizeof msg, fmt, args);
    va_end(args);
    error_semantico(n->linea, n->columna, "%s", msg);
    notas_de_contexto();
}

static void advertencia_en(const Nodo *n, const char *fmt, ...)
{
    char msg[512];
    va_list args;

    if (ya_reportado(n))
        return;
    va_start(args, fmt);
    vsnprintf(msg, sizeof msg, fmt, args);
    va_end(args);
    advertencia(n->linea, n->columna, "%s", msg);
    notas_de_contexto();
}

/* " (fila = 3, i = 0)" con los indices de repeat activos, o "". */
static void sufijo_indices(char *buf, size_t tam)
{
    size_t largo = 0;
    int i;

    buf[0] = '\0';
    for (i = 0; i < n_indices && largo < tam; i++)
        largo += snprintf(buf + largo, tam - largo, "%s%s = %g",
                          i == 0 ? " (con " : ", ",
                          indices[i]->nombre, indices[i]->valor);
    if (n_indices > 0 && largo < tam)
        snprintf(buf + largo, tam - largo, ")");
}

/* Por que una expresion no es constante, a partir del nombre que la hizo
 * fallar en evaluar(). */
static void describir_no_constante(const Nodo *culpable, char *buf, size_t tam)
{
    const Simbolo *s = culpable->simbolo;

    if (s && s->clase == S_VAR)
        snprintf(buf, tam, "depende de la variable '%s'", s->nombre);
    else if (s && s->clase == S_PARAM)
        snprintf(buf, tam, "depende del parametro '%s', que en esta llamada "
                 "no recibe un valor constante", s->nombre);
    else
        snprintf(buf, tam, "no es una expresion constante");
}

/* Evalua una expresion y, si tiene una division por cero, la reporta. */
static ResultadoEval evaluar_y_revisar(const Nodo *e, double *v,
                                       const Nodo **culpable)
{
    ResultadoEval r = evaluar(e, v, culpable);

    if (r == EVAL_DIV_CERO)
        error_en(*culpable, "division o resto por cero");
    return r;
}

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

static void verificar_limite(const Nodo *arg, int i, ResultadoEval r, double v,
                             const Nodo *culpable)
{
    static const char *const campo[3] = {
        CAMPO_LIMITE_X, CAMPO_LIMITE_Y, CAMPO_LIMITE_Z
    };
    char extra[256];

    if (r == EVAL_OK) {
        if (v < 0 || v > limite[i]) {
            sufijo_indices(extra, sizeof extra);
            error_en(arg, "%c = %g esta fuera de los limites de la maquina: "
                     "tiene que estar entre 0 y %g (%s.%s)%s", NOMBRE_EJE[i],
                     v, limite[i], nombre_maquina, campo[i], extra);
        }
    } else if (r == EVAL_NO_CONSTANTE) {
        describir_no_constante(culpable, extra, sizeof extra);
        advertencia_en(arg, "no se puede verificar en compilacion que %c quede "
                       "entre 0 y %g: %s", NOMBRE_EJE[i], limite[i], extra);
    }
}

static void verificar_feedrate(const Nodo *arg, ResultadoEval r, double v,
                               const Nodo *culpable)
{
    char extra[256];

    if (r == EVAL_OK) {
        sufijo_indices(extra, sizeof extra);
        if (v <= 0)
            error_en(arg, "F tiene que ser mayor que 0, y vale %g%s", v, extra);
        else if (v > feedrate_max)
            error_en(arg, "F = %g supera el feedrate maximo de la maquina "
                     "(%s.%s = %g)%s", v, nombre_maquina, CAMPO_FEEDRATE_MAX,
                     feedrate_max, extra);
    } else if (r == EVAL_NO_CONSTANTE) {
        describir_no_constante(culpable, extra, sizeof extra);
        advertencia_en(arg, "no se puede verificar en compilacion que F no "
                       "supere %g: %s", feedrate_max, extra);
    }
}

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
            i = indice_eje(arg->arg_eje.eje);
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
            error_en(s, "'%s' con posicion desconocida en %s %s: %s",
                     b->nombre, cuantos > 1 ? "los ejes" : "el eje", ejes,
                     causa);
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
        r = evaluar_y_revisar(valor, &v, &culpable);
        if (arg->tipo != N_ARG_EJE)
            continue;

        i = indice_eje(arg->arg_eje.eje);
        if (b->efecto == EF_MUEVE && i >= 0)
            verificar_limite(arg, i, r, v, culpable);
        else if (arg->arg_eje.eje == 'F')
            verificar_feedrate(arg, r, v, culpable);
    }

    /* Efecto sobre la posicion. */
    switch (b->efecto) {
    case EF_HOMING:
        /* AutoHome() referencia los tres ejes; AutoHome(X, Y) solo esos. */
        for (i = 0; i < 3; i++) {
            int nombrado = s->llamada.args == NULL;
            for (arg = s->llamada.args; arg; arg = arg->sig)
                if (arg->tipo == N_ARG_EJE && indice_eje(arg->arg_eje.eje) == i)
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
/* Procedimientos: expansion en linea                                  */
/* ------------------------------------------------------------------ */

static void ejecutar_proc(Nodo *s, Simbolo *p, Estado *e)
{
    Nodo *decl = p->decl;
    Nodo *param, *arg;
    int n = 0, i;
    double *valores;
    int *conocidos;

    /* Resguardo: el primer recorrido ya prohibe la recursion. */
    for (i = 0; i < n_pila; i++)
        if (pila[i]->simbolo == p) {
            error_en(s, "'%s' se llama recursivamente", p->nombre);
            return;
        }

    for (param = decl->proc.params; param; param = param->sig)
        n++;
    valores   = calloc(n ? n : 1, sizeof *valores);
    conocidos = calloc(n ? n : 1, sizeof *conocidos);
    if (!valores || !conocidos) {
        fprintf(stderr, "sin memoria en el analisis semantico\n");
        exit(2);
    }

    /* Los argumentos se evaluan en el contexto de quien llama, antes de
     * ligar los parametros. Un parametro machine no necesita valor: es
     * siempre la maquina del programa. */
    for (param = decl->proc.params, arg = s->llamada.args, i = 0;
         param && arg; param = param->sig, arg = arg->sig, i++) {
        const Nodo *culpable;
        if (param->param.tipo != DATO_MACHINE)
            conocidos[i] = evaluar_y_revisar(arg, &valores[i], &culpable)
                           == EVAL_OK;
    }

    /* Como no hay recursion, cada parametro tiene a lo sumo una llamada
     * activa: su valor puede guardarse en la propia entrada de la tabla. */
    for (param = decl->proc.params, i = 0; param; param = param->sig, i++) {
        param->simbolo->conocido = conocidos[i];
        param->simbolo->valor    = valores[i];
    }

    if (n_pila == cap_pila)
        pila = agrandar(pila, &cap_pila, sizeof *pila);
    pila[n_pila++] = s;

    ejecutar(decl->proc.cuerpo, e);

    n_pila--;
    for (param = decl->proc.params; param; param = param->sig)
        param->simbolo->conocido = 0;
    free(valores);
    free(conocidos);
}

/* ------------------------------------------------------------------ */
/* Sentencias                                                          */
/* ------------------------------------------------------------------ */

static void ejecutar_repeat(Nodo *s, Estado *e)
{
    Simbolo *indice = s->repetir.indice ? s->simbolo : NULL;
    const Nodo *culpable = NULL;
    const char *problema;
    char extra[256];
    double v = 0;
    long vueltas, k;

    switch (evaluar_y_revisar(s->repetir.veces, &v, &culpable)) {
    case EVAL_DIV_CERO:
        return;
    case EVAL_NO_CONSTANTE:
        describir_no_constante(culpable, extra, sizeof extra);
        error_en(s->repetir.veces, "la cantidad de repeticiones tiene que ser "
                 "constante, y %s", extra);
        return;
    case EVAL_OK:
        break;
    }
    problema = problema_repeticiones(v);
    if (problema) {
        error_en(s->repetir.veces, "la cantidad de repeticiones %s, y vale %g",
                 problema, v);
        return;
    }

    /* El cuerpo se recorre una vez por vuelta, igual que el desenrollado.
     * El indice toma los valores 0, 1, ..., vueltas - 1. */
    if (indice) {
        if (n_indices == cap_indices)
            indices = agrandar(indices, &cap_indices, sizeof *indices);
        indices[n_indices++] = indice;
    }
    vueltas = (long) v;
    for (k = 0; k < vueltas; k++) {
        if (indice) {
            indice->conocido = 1;
            indice->valor    = (double) k;
        }
        ejecutar(s->repetir.cuerpo, e);
    }
    if (indice) {
        indice->conocido = 0;
        n_indices--;
    }
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
        evaluar_y_revisar(s->decl.valor, &v, &culpable);
        break;

    case N_ASIGNACION:
        evaluar_y_revisar(s->asignacion.valor, &v, &culpable);
        break;

    case N_IF: {
        /* Cada rama arranca con el estado de antes del if; si no hay else,
         * la rama vacia lo deja igual. Despues se unen. */
        Estado entonces = *e, sino = *e;

        evaluar_y_revisar(s->si.cond, &v, &culpable);
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
        if (s->simbolo->clase == S_BUILTIN)
            ejecutar_builtin(s, s->simbolo->builtin, e);
        else if (s->simbolo->clase == S_PROC)
            ejecutar_proc(s, s->simbolo, e);
        break;

    default:
        break;
    }
}

/* ------------------------------------------------------------------ */
/* Entrada                                                             */
/* ------------------------------------------------------------------ */

static double campo_maquina(const Simbolo *m, const char *nombre)
{
    const Simbolo *f = alcance_buscar_local(m->campos, nombre);
    return f ? f->valor : 0;
}

void analizar_posicion(Nodo *programa)
{
    Nodo *decl = programa->programa.declaraciones;
    const Simbolo *m;
    Estado e;
    int i;

    if (!decl || decl->tipo != N_MAQUINA || !decl->simbolo)
        return;   /* no pasa si el primer recorrido termino bien */

    m = decl->simbolo;
    nombre_maquina = m->nombre;
    limite[0]    = campo_maquina(m, CAMPO_LIMITE_X);
    limite[1]    = campo_maquina(m, CAMPO_LIMITE_Y);
    limite[2]    = campo_maquina(m, CAMPO_LIMITE_Z);
    feedrate_max = campo_maquina(m, CAMPO_FEEDRATE_MAX);

    /* Al encender, la impresora no sabe donde esta: los tres ejes
     * arrancan con posicion desconocida. */
    for (i = 0; i < 3; i++) {
        e.eje[i].conocida = 0;
        e.eje[i].causa    = NULL;
        e.eje[i].si       = NULL;
    }

    ejecutar(programa->programa.principal, &e);

    while (reportados) {
        Reportado *sig = reportados->sig;
        free(reportados->pila);
        free(reportados);
        reportados = sig;
    }
    free(pila);
    free(indices);
    pila = NULL;
    indices = NULL;
    n_pila = cap_pila = n_indices = cap_indices = 0;
}
