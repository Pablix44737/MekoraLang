#ifndef EVALUADOR_H
#define EVALUADOR_H

#include "ast.h"

/* evaluador.h - Evaluacion de expresiones en compilacion
 *
 * Constante: literales, const, campos, indices y parametros con argumento
 * constante (spec 8.3). Las variables tienen valor recien al generar.
 * bool = 0 o 1. and y or sin cortocircuito. */

typedef enum {
    EVAL_OK,             /* *valor tiene el resultado                    */
    EVAL_NO_CONSTANTE,   /* depende de algo sin valor: *culpable lo dice */
    EVAL_DIV_CERO        /* division o resto por cero: *culpable es la
                            operacion                                    */
} ResultadoEval;

ResultadoEval evaluar(const Nodo *e, double *valor, const Nodo **culpable);

#endif
