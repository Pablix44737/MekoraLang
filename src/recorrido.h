#ifndef RECORRIDO_H
#define RECORRIDO_H

#include <stddef.h>
#include "ast.h"
#include "builtins.h"
#include "evaluador.h"
#include "simbolos.h"

/* recorrido.h - Lo que comparten los recorridos que ejecutan el programa
 *
 * El segundo recorrido del analisis semantico (posicion.c) y la generacion
 * de codigo (codegen.c) recorren el programa como se va a ejecutar: empiezan
 * en main, entran en cada llamada y repiten el cuerpo de cada repeat. Este
 * modulo tiene lo que necesitan los dos: la pila de llamadas, el valor de
 * los parametros y de los indices, los datos de la maquina, la verificacion
 * de limites y feedrate, y el registro de lo ya reportado.
 *
 * Lo que se reporta en un recorrido queda registrado hasta recorrido_liberar:
 * la generacion consulta ahi donde dio advertencia el analisis. */

/* error_semantico o error_generacion, segun quien recorre. */
typedef void (*ReportarError)(int linea, int columna, const char *fmt, ...);

/* Carga los datos de la maquina y elige como se reportan los errores.
 * Devuelve 0 si el programa no tiene maquina, lo que no pasa si el primer
 * recorrido termino bien. */
int  recorrido_iniciar(Nodo *programa, ReportarError reportar);
void recorrido_liberar(void);

/* Cada error o advertencia se reporta una vez por sentencia y por cadena de
 * llamadas, seguido de una nota por cada llamada de la cadena. */
void recorrido_error(const Nodo *n, const char *fmt, ...);
void recorrido_advertencia(const Nodo *n, const char *fmt, ...);

/* Devuelve 1 si el analisis dio una advertencia sobre ese nodo con la cadena
 * de llamadas actual. */
int  recorrido_advertido(const Nodo *n);

/* Evalua una expresion y, si tiene una division por cero, la reporta. */
ResultadoEval recorrido_evaluar(const Nodo *e, double *v, const Nodo **culpable);

/* Por que una expresion no es constante, a partir del nombre que la hizo
 * fallar en evaluar(). */
void recorrido_describir_no_constante(const Nodo *culpable, char *buf, size_t tam);

/* 0, 1 y 2 para X, Y y Z; -1 para cualquier otro eje. */
int  recorrido_indice_eje(char eje);

/* Verifica un argumento de eje de un builtin: limites en X, Y, Z de Move y
 * Draw, y feedrate en todo F. r, v y culpable son el resultado de evaluarlo. */
void recorrido_verificar(const Builtin *b, const Nodo *arg, ResultadoEval r,
                         double v, const Nodo *culpable);

/* Evalua los argumentos en el contexto de quien llama, liga los parametros y
 * apila la llamada. Devuelve 0 si la llamada es recursiva, y entonces no hay
 * que recorrer el cuerpo ni llamar a recorrido_salir_proc. */
int  recorrido_entrar_proc(Nodo *llamada, Simbolo *p);
void recorrido_salir_proc(Simbolo *p);

/* Cantidad de vueltas de un repeat. Devuelve 0 si hubo un error, ya
 * reportado. */
int  recorrido_vueltas(const Nodo *repeat, long *vueltas);

/* El indice es NULL en un repeat sin "as". */
void recorrido_entrar_repeat(Simbolo *indice);
void recorrido_vuelta(Simbolo *indice, long k);
void recorrido_salir_repeat(Simbolo *indice);

#endif
