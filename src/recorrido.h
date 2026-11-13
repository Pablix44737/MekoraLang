#ifndef RECORRIDO_H
#define RECORRIDO_H

#include <stddef.h>
#include "ast.h"
#include "builtins.h"
#include "evaluador.h"
#include "simbolos.h"

/* recorrido.h - Lo comun a posicion.c y codegen.c
 *
 * Pila de llamadas, parametros, indices, datos de la maquina, limites y
 * errores ya reportados (la generacion los consulta para los comentarios). */

/* error_semantico o error_generacion, segun quien recorre. */
typedef void (*ReportarError)(int linea, int columna, const char *fmt, ...);

/* Carga la maquina y la funcion de error. 0 si no hay maquina. */
int  recorrido_iniciar(Nodo *programa, ReportarError reportar);
void recorrido_liberar(void);

/* Una vez por sentencia y cadena de llamadas, con notas */
void recorrido_error(const Nodo *n, const char *fmt, ...);
void recorrido_advertencia(const Nodo *n, const char *fmt, ...);

/* 1 si el analisis advirtio sobre ese nodo en esta cadena */
int  recorrido_advertido(const Nodo *n);

/* Evalua una expresion y, si tiene una division por cero, la reporta. */
ResultadoEval recorrido_evaluar(const Nodo *e, double *v, const Nodo **culpable);

/* Mensaje de por que no es constante */
void recorrido_describir_no_constante(const Nodo *culpable, char *buf, size_t tam);

/* 0, 1 y 2 para X, Y y Z; -1 para cualquier otro eje. */
int  recorrido_indice_eje(char eje);

/* Limites (X, Y, Z de Move y Draw) y feedrate (F) */
void recorrido_verificar(const Builtin *b, const Nodo *arg, ResultadoEval r,
                         double v, const Nodo *culpable);

/* Liga los parametros y apila la llamada. 0 si es recursiva. */
int  recorrido_entrar_proc(Nodo *llamada, Simbolo *p);
void recorrido_salir_proc(Simbolo *p);

/* Vueltas de un repeat. 0 si hubo error. */
int  recorrido_vueltas(const Nodo *repeat, long *vueltas);

/* El indice es NULL en un repeat sin "as". */
void recorrido_entrar_repeat(Simbolo *indice);
void recorrido_vuelta(Simbolo *indice, long k);
void recorrido_salir_repeat(Simbolo *indice);

#endif
