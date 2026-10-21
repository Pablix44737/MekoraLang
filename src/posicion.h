#ifndef POSICION_H
#define POSICION_H

#include "ast.h"

/* posicion.h - Analisis semantico, segundo recorrido
 *
 * Recorre el programa como se va a ejecutar: empieza en main y, en cada
 * llamada, entra al cuerpo del procedimiento con el estado actual, igual
 * que la expansion en linea que hace el generador. Cada repeat se recorre
 * tantas veces como se repite. En ese recorrido hace los chequeos que
 * dependen del camino: posicion conocida (seccion 8.1 de la
 * especificacion), limites de la maquina y feedrate maximo (8.3), y el
 * valor de cada repeat.
 *
 * Supone que el primer recorrido termino sin errores: el arbol esta
 * decorado y la primera declaracion es la maquina. */

void analizar_posicion(Nodo *programa);

#endif
