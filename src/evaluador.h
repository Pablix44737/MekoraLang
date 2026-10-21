#ifndef EVALUADOR_H
#define EVALUADOR_H

#include "ast.h"

/* evaluador.h - Evaluacion de expresiones en compilacion
 *
 * Calcula el valor de una expresion ya analizada (el primer recorrido dejo
 * cada nombre apuntando a su entrada de la tabla de simbolos). Una
 * expresion es constante si solo usa literales, constantes, campos de la
 * maquina, indices de repeat y parametros cuyo argumento fue constante en
 * la llamada (especificacion, seccion 8.3). Las variables nunca lo son,
 * aunque se pueda deducir su valor, porque se pueden reasignar.
 *
 * Los bool se representan con 0 y 1. */

typedef enum {
    EVAL_OK,             /* *valor tiene el resultado                    */
    EVAL_NO_CONSTANTE,   /* depende de algo sin valor: *culpable lo dice */
    EVAL_DIV_CERO        /* division o resto por cero: *culpable es la
                            operacion                                    */
} ResultadoEval;

ResultadoEval evaluar(const Nodo *e, double *valor, const Nodo **culpable);

#endif
