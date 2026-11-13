#include <math.h>
#include "codegen.h"
#include "builtins.h"
#include "errores.h"
#include "evaluador.h"
#include "recorrido.h"
#include "simbolos.h"

/* codegen.c - Generacion de G-code (tercer recorrido, desde main)
 *
 * No hay entradas, asi que se ejecuta el programa: repeat se desenrolla,
 * los procedimientos se expanden, las variables guardan su valor en la
 * tabla y de cada if sale solo la rama que se toma. Limites y feedrate se
 * vuelven a verificar con los valores reales. */

static FILE *salida;

static void generar_sentencia(Nodo *s);

void emitir_prologo(FILE *out)
{
    fprintf(out, "; Generado automaticamente - no editar\n");
    fprintf(out, "G21\n");   /* unidades en milimetros */
    fprintf(out, "G90\n");   /* posicionamiento absoluto (X, Y, Z) */
    fprintf(out, "M83\n");   /* extrusion relativa (E) */
}

void emitir_epilogo(FILE *out)
{
    fprintf(out, "M104 S0\n");   /* apagar extrusor */
    fprintf(out, "M140 S0\n");   /* apagar cama */
    fprintf(out, "M107\n");      /* apagar ventilador */
    fprintf(out, "M84\n");       /* deshabilitar motores */
}

/* --- Builtins --- */

/* XYZEF con 3 decimales; S sin decimales si es entero */
static void emitir_valor(char letra, double v)
{
    if (fabs(v) < 0.0005)
        v = 0;   /* que no salga "-0.000" */
    if (letra == 'S' && v == floor(v))
        fprintf(salida, "%.0f", v);
    else
        fprintf(salida, "%.3f", v);
}

/* Argumento de la llamada para ese parametro, o NULL. "posicion" cuenta
 * los posicionales ya vistos. */
static const Nodo *argumento(const Nodo *args, const ParamBuiltin *p,
                             int *posicion)
{
    const Nodo *a;
    int saltear = p->eje ? 0 : (*posicion)++;

    for (a = args; a; a = a->sig) {
        if (p->eje) {
            if (a->tipo == N_ARG_EJE && a->arg_eje.eje == p->eje)
                return a;
        } else if (a->tipo != N_ARG_EJE && saltear-- == 0) {
            return a;
        }
    }
    return NULL;
}

/* Plantilla + parametros en el orden de la tabla */
static void emitir_builtin(const Nodo *s, const Builtin *b)
{
    const Nodo *args[MAX_PARAMS_BUILTIN];
    double valores[MAX_PARAMS_BUILTIN];
    int posicion = 0;
    int completo = 1;
    int i;

    /* primero se evalua todo: los comentarios van antes de la linea */
    for (i = 0; i < b->n_params; i++) {
        const ParamBuiltin *p = &b->params[i];
        const Nodo *valor;
        const Nodo *culpable = NULL;
        ResultadoEval r;

        args[i] = argumento(s->llamada.args, p, &posicion);
        if (!args[i] || p->clase == P_CADENA || p->clase == P_EJE_SOLO)
            continue;

        valor = p->clase == P_NOMBRADO ? args[i]->arg_eje.valor : args[i];
        r = recorrido_evaluar(valor, &valores[i], &culpable);
        if (r != EVAL_OK) {
            completo = 0;
            continue;
        }
        if (p->clase == P_NOMBRADO) {
            recorrido_verificar(b, args[i], r, valores[i], culpable);
            if (recorrido_advertido(args[i]))
                fprintf(salida, "; advertencia (linea %d): %c depende de una "
                        "variable\n", args[i]->linea, p->eje);
        }
    }
    if (!completo)
        return;

    fputs(b->plantilla, salida);
    for (i = 0; i < b->n_params; i++) {
        const ParamBuiltin *p = &b->params[i];

        if (!args[i])
            continue;
        if (p->clase == P_CADENA) {
            fprintf(salida, "%s%s", b->plantilla[0] ? " " : "", args[i]->cadena);
        } else if (p->clase == P_EJE_SOLO) {
            fprintf(salida, " %c", p->letra);
        } else {
            fprintf(salida, " %c", p->letra);
            emitir_valor(p->letra, valores[i]);
        }
    }
    fputc('\n', salida);
}

/* --- Sentencias --- */

/* El valor va en la entrada de la tabla (sin recursion alcanza una) */
static void asignar(Simbolo *v, const Nodo *valor)
{
    const Nodo *culpable;

    v->conocido = recorrido_evaluar(valor, &v->valor, &culpable) == EVAL_OK;
}

static void generar_if(Nodo *s)
{
    const Nodo *culpable;
    double cond;

    if (recorrido_evaluar(s->si.cond, &cond, &culpable) != EVAL_OK)
        return;
    if (cond != 0)
        generar_sentencia(s->si.entonces);
    else if (s->si.sino)
        generar_sentencia(s->si.sino);
}

static void generar_repeat(Nodo *s)
{
    Simbolo *indice = s->repetir.indice ? s->simbolo : NULL;
    long vueltas, k;

    if (!recorrido_vueltas(s, &vueltas))
        return;

    recorrido_entrar_repeat(indice);
    for (k = 0; k < vueltas; k++) {
        recorrido_vuelta(indice, k);
        generar_sentencia(s->repetir.cuerpo);
    }
    recorrido_salir_repeat(indice);
}

static void generar_sentencia(Nodo *s)
{
    switch (s->tipo) {
    case N_BLOQUE:
        for (s = s->bloque.sentencias; s; s = s->sig)
            generar_sentencia(s);
        break;

    case N_VAR:
        asignar(s->simbolo, s->decl.valor);
        break;

    case N_ASIGNACION:
        asignar(s->simbolo, s->asignacion.valor);
        break;

    case N_IF:
        generar_if(s);
        break;

    case N_REPEAT:
        generar_repeat(s);
        break;

    case N_LLAMADA:
        if (s->simbolo->clase == S_BUILTIN) {
            emitir_builtin(s, s->simbolo->builtin);
        } else if (s->simbolo->clase == S_PROC &&
                   recorrido_entrar_proc(s, s->simbolo)) {
            generar_sentencia(s->simbolo->decl->proc.cuerpo);
            recorrido_salir_proc(s->simbolo);
        }
        break;

    default:
        break;
    }
}

void generar(Nodo *programa, FILE *out)
{
    if (!recorrido_iniciar(programa, error_generacion))
        return;   /* no pasa si el analisis termino bien */

    salida = out;
    generar_sentencia(programa->programa.principal);
}
