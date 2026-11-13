#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "recorrido.h"
#include "semantico.h"
#include "errores.h"

static ReportarError reportar_error;

/* Datos de la maquina. */
static const char *nombre_maquina;
static double      limite[3];
static double      feedrate_max;

/* Pila de llamadas, para las notas de los errores */
static Nodo **pila;
static int    n_pila, cap_pila;

/* Indices activos, para mostrar su valor en los mensajes */
static Simbolo **indices;
static int       n_indices, cap_indices;

/* Lo ya reportado: una vez por sentencia y cadena de llamadas */
typedef enum { R_ERROR, R_ADVERTENCIA } ClaseReporte;

typedef struct Reportado {
    const Nodo        *nodo;
    ClaseReporte       clase;
    int                n;
    Nodo             **pila;
    struct Reportado  *sig;
} Reportado;

static Reportado *reportados;

/* --- Utilidades --- */

static void sin_memoria(void)
{
    fprintf(stderr, "sin memoria al recorrer el programa\n");
    exit(2);
}

static void *agrandar(void *p, int *cap, size_t tam)
{
    *cap = *cap ? *cap * 2 : 8;
    p = realloc(p, *cap * tam);
    if (!p)
        sin_memoria();
    return p;
}

int recorrido_indice_eje(char eje)
{
    switch (eje) {
    case 'X': return 0;
    case 'Y': return 1;
    case 'Z': return 2;
    default:  return -1;
    }
}

static double campo_maquina(const Simbolo *m, const char *nombre)
{
    const Simbolo *f = alcance_buscar_local(m->campos, nombre);
    return f ? f->valor : 0;
}

int recorrido_iniciar(Nodo *programa, ReportarError reportar)
{
    Nodo *decl = programa->programa.declaraciones;
    const Simbolo *m;

    if (!decl || decl->tipo != N_MAQUINA || !decl->simbolo)
        return 0;

    m = decl->simbolo;
    nombre_maquina = m->nombre;
    limite[0]    = campo_maquina(m, CAMPO_LIMITE_X);
    limite[1]    = campo_maquina(m, CAMPO_LIMITE_Y);
    limite[2]    = campo_maquina(m, CAMPO_LIMITE_Z);
    feedrate_max = campo_maquina(m, CAMPO_FEEDRATE_MAX);
    reportar_error = reportar;
    return 1;
}

void recorrido_liberar(void)
{
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

/* --- Reportes --- */

static Reportado *buscar_reportado(const Nodo *nodo, ClaseReporte clase)
{
    Reportado *r;

    for (r = reportados; r; r = r->sig)
        if (r->nodo == nodo && r->clase == clase && r->n == n_pila &&
            (n_pila == 0 || memcmp(r->pila, pila, n_pila * sizeof *pila) == 0))
            return r;
    return NULL;
}

/* 1 si ya se reporto; si no, lo anota */
static int ya_reportado(const Nodo *nodo, ClaseReporte clase)
{
    Reportado *r;

    if (buscar_reportado(nodo, clase))
        return 1;

    r = malloc(sizeof *r);
    if (!r)
        sin_memoria();
    r->nodo  = nodo;
    r->clase = clase;
    r->n     = n_pila;
    r->pila  = malloc((n_pila ? n_pila : 1) * sizeof *pila);
    if (!r->pila)
        sin_memoria();
    if (n_pila > 0)
        memcpy(r->pila, pila, n_pila * sizeof *pila);
    r->sig = reportados;
    reportados = r;
    return 0;
}

int recorrido_advertido(const Nodo *n)
{
    return buscar_reportado(n, R_ADVERTENCIA) != NULL;
}

/* Una nota por cada llamada de la cadena, de la mas interna a main. */
static void notas_de_contexto(void)
{
    int i;

    for (i = n_pila - 1; i >= 0; i--)
        nota(pila[i]->linea, pila[i]->columna, "en la llamada a '%s'",
             pila[i]->llamada.nombre);
}

void recorrido_error(const Nodo *n, const char *fmt, ...)
{
    char msg[512];
    va_list args;

    if (ya_reportado(n, R_ERROR))
        return;
    va_start(args, fmt);
    vsnprintf(msg, sizeof msg, fmt, args);
    va_end(args);
    reportar_error(n->linea, n->columna, "%s", msg);
    notas_de_contexto();
}

void recorrido_advertencia(const Nodo *n, const char *fmt, ...)
{
    char msg[512];
    va_list args;

    if (ya_reportado(n, R_ADVERTENCIA))
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

void recorrido_describir_no_constante(const Nodo *culpable, char *buf, size_t tam)
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

ResultadoEval recorrido_evaluar(const Nodo *e, double *v, const Nodo **culpable)
{
    ResultadoEval r = evaluar(e, v, culpable);

    if (r == EVAL_DIV_CERO)
        recorrido_error(*culpable, "division o resto por cero");
    return r;
}

/* --- Limites y feedrate --- */

static void verificar_limite(const Nodo *arg, int i, ResultadoEval r, double v,
                             const Nodo *culpable)
{
    static const char NOMBRE_EJE[3] = { 'X', 'Y', 'Z' };
    static const char *const campo[3] = {
        CAMPO_LIMITE_X, CAMPO_LIMITE_Y, CAMPO_LIMITE_Z
    };
    char extra[256];

    if (r == EVAL_OK) {
        if (v < 0 || v > limite[i]) {
            sufijo_indices(extra, sizeof extra);
            recorrido_error(arg, "%c = %g esta fuera de los limites de la "
                            "maquina: tiene que estar entre 0 y %g (%s.%s)%s",
                            NOMBRE_EJE[i], v, limite[i], nombre_maquina,
                            campo[i], extra);
        }
    } else if (r == EVAL_NO_CONSTANTE) {
        recorrido_describir_no_constante(culpable, extra, sizeof extra);
        recorrido_advertencia(arg, "no se puede verificar en compilacion que "
                              "%c quede entre 0 y %g: %s", NOMBRE_EJE[i],
                              limite[i], extra);
    }
}

static void verificar_feedrate(const Nodo *arg, ResultadoEval r, double v,
                               const Nodo *culpable)
{
    char extra[256];

    if (r == EVAL_OK) {
        sufijo_indices(extra, sizeof extra);
        if (v <= 0)
            recorrido_error(arg, "F tiene que ser mayor que 0, y vale %g%s",
                            v, extra);
        else if (v > feedrate_max)
            recorrido_error(arg, "F = %g supera el feedrate maximo de la "
                            "maquina (%s.%s = %g)%s", v, nombre_maquina,
                            CAMPO_FEEDRATE_MAX, feedrate_max, extra);
    } else if (r == EVAL_NO_CONSTANTE) {
        recorrido_describir_no_constante(culpable, extra, sizeof extra);
        recorrido_advertencia(arg, "no se puede verificar en compilacion que "
                              "F no supere %g: %s", feedrate_max, extra);
    }
}

void recorrido_verificar(const Builtin *b, const Nodo *arg, ResultadoEval r,
                         double v, const Nodo *culpable)
{
    int i = recorrido_indice_eje(arg->arg_eje.eje);

    if (b->efecto == EF_MUEVE && i >= 0)
        verificar_limite(arg, i, r, v, culpable);
    else if (arg->arg_eje.eje == 'F')
        verificar_feedrate(arg, r, v, culpable);
}

/* --- Procedimientos: expansion en linea --- */

int recorrido_entrar_proc(Nodo *llamada, Simbolo *p)
{
    Nodo *decl = p->decl;
    Nodo *param, *arg;
    int n = 0, i;
    double *valores;
    int *conocidos;

    /* Resguardo: el primer recorrido ya prohibe la recursion. */
    for (i = 0; i < n_pila; i++)
        if (pila[i]->simbolo == p) {
            recorrido_error(llamada, "'%s' se llama recursivamente", p->nombre);
            return 0;
        }

    for (param = decl->proc.params; param; param = param->sig)
        n++;
    valores   = calloc(n ? n : 1, sizeof *valores);
    conocidos = calloc(n ? n : 1, sizeof *conocidos);
    if (!valores || !conocidos)
        sin_memoria();

    /* argumentos con los valores de quien llama (machine no tiene valor) */
    for (param = decl->proc.params, arg = llamada->llamada.args, i = 0;
         param && arg; param = param->sig, arg = arg->sig, i++) {
        const Nodo *culpable;
        if (param->param.tipo != DATO_MACHINE)
            conocidos[i] = recorrido_evaluar(arg, &valores[i], &culpable)
                           == EVAL_OK;
    }

    /* sin recursion, el valor va en la entrada del parametro */
    for (param = decl->proc.params, i = 0; param; param = param->sig, i++) {
        param->simbolo->conocido = conocidos[i];
        param->simbolo->valor    = valores[i];
    }
    free(valores);
    free(conocidos);

    if (n_pila == cap_pila)
        pila = agrandar(pila, &cap_pila, sizeof *pila);
    pila[n_pila++] = llamada;
    return 1;
}

void recorrido_salir_proc(Simbolo *p)
{
    Nodo *param;

    n_pila--;
    for (param = p->decl->proc.params; param; param = param->sig)
        param->simbolo->conocido = 0;
}

/* --- Repeat: desenrollado --- */

int recorrido_vueltas(const Nodo *repeat, long *vueltas)
{
    const Nodo *culpable = NULL;
    const char *problema;
    char extra[256];
    double v = 0;

    switch (recorrido_evaluar(repeat->repetir.veces, &v, &culpable)) {
    case EVAL_DIV_CERO:
        return 0;
    case EVAL_NO_CONSTANTE:
        recorrido_describir_no_constante(culpable, extra, sizeof extra);
        recorrido_error(repeat->repetir.veces, "la cantidad de repeticiones "
                        "tiene que ser constante, y %s", extra);
        return 0;
    case EVAL_OK:
        break;
    }
    problema = problema_repeticiones(v);
    if (problema) {
        recorrido_error(repeat->repetir.veces, "la cantidad de repeticiones "
                        "%s, y vale %g", problema, v);
        return 0;
    }
    *vueltas = (long) v;
    return 1;
}

void recorrido_entrar_repeat(Simbolo *indice)
{
    if (!indice)
        return;
    if (n_indices == cap_indices)
        indices = agrandar(indices, &cap_indices, sizeof *indices);
    indices[n_indices++] = indice;
}

/* El indice toma los valores 0, 1, ..., vueltas - 1. */
void recorrido_vuelta(Simbolo *indice, long k)
{
    if (!indice)
        return;
    indice->conocido = 1;
    indice->valor    = (double) k;
}

void recorrido_salir_repeat(Simbolo *indice)
{
    if (!indice)
        return;
    indice->conocido = 0;
    n_indices--;
}
