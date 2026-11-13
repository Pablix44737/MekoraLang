#ifndef POSICION_H
#define POSICION_H

#include "ast.h"

/* posicion.h - Segundo recorrido del analisis semantico
 *
 * Desde main, como se ejecuta: posicion conocida, limites, feedrate y
 * cantidad de cada repeat. Supone el primer recorrido sin errores. */

void analizar_posicion(Nodo *programa);

#endif
