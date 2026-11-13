#include <math.h>
#include "evaluador.h"
#include "simbolos.h"

static double operar(Operador op, double a, double b)
{
    switch (op) {
    case OP_SUMA:        return a + b;
    case OP_RESTA:       return a - b;
    case OP_MULT:        return a * b;
    case OP_DIV:         return a / b;
    case OP_MOD:         return fmod(a, b);   /* resto real: 7.5 % 2 = 1.5 */
    case OP_IGUAL:       return a == b;
    case OP_DISTINTO:    return a != b;
    case OP_MENOR:       return a < b;
    case OP_MAYOR:       return a > b;
    case OP_MENOR_IGUAL: return a <= b;
    case OP_MAYOR_IGUAL: return a >= b;
    case OP_AND:         return a != 0 && b != 0;
    case OP_OR:          return a != 0 || b != 0;
    case OP_NOT:
    case OP_NEG:         break;   /* unarios, no llegan aca */
    }
    return 0;
}

ResultadoEval evaluar(const Nodo *e, double *valor, const Nodo **culpable)
{
    ResultadoEval ri, rd;
    double vi, vd;
    const Nodo *ci, *cd;

    switch (e->tipo) {
    case N_NUMERO:
        *valor = e->numero;
        return EVAL_OK;

    case N_BOOL:
        *valor = e->booleano;
        return EVAL_OK;

    case N_ID:
    case N_ACCESO:
        /* N_ACCESO apunta directamente al campo de la maquina. */
        if (e->simbolo && e->simbolo->conocido) {
            *valor = e->simbolo->valor;
            return EVAL_OK;
        }
        *culpable = e;
        return EVAL_NO_CONSTANTE;

    case N_UNARIA:
        ri = evaluar(e->unaria.operando, &vi, culpable);
        if (ri != EVAL_OK)
            return ri;
        *valor = e->unaria.op == OP_NEG ? -vi : (vi == 0);
        return EVAL_OK;

    case N_BINARIA:
        /* los dos lados siempre, para no perder una division por cero */
        ri = evaluar(e->binaria.izq, &vi, &ci);
        rd = evaluar(e->binaria.der, &vd, &cd);
        if (ri == EVAL_DIV_CERO) { *culpable = ci; return ri; }
        if (rd == EVAL_DIV_CERO) { *culpable = cd; return rd; }
        if (rd == EVAL_OK && vd == 0 &&
            (e->binaria.op == OP_DIV || e->binaria.op == OP_MOD)) {
            *culpable = e;
            return EVAL_DIV_CERO;
        }
        if (ri != EVAL_OK) { *culpable = ci; return ri; }
        if (rd != EVAL_OK) { *culpable = cd; return rd; }
        *valor = operar(e->binaria.op, vi, vd);
        return EVAL_OK;

    default:
        /* cadenas y ejes no llegan aca */
        *culpable = e;
        return EVAL_NO_CONSTANTE;
    }
}
