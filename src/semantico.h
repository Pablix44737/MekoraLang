#ifndef SEMANTICO_H
#define SEMANTICO_H

#include "ast.h"

/* semantico.h - Primer recorrido del analisis semantico
 *
 * Tabla de simbolos, nombres, tipos y llamadas. Deja el arbol decorado. */

/* Campos obligatorios de la maquina (spec 5.1) */
#define CAMPO_LIMITE_X      "limite_x"
#define CAMPO_LIMITE_Y      "limite_y"
#define CAMPO_LIMITE_Z      "limite_z"
#define CAMPO_FEEDRATE_MAX  "feedrate_max"

void analizar_semantica(Nodo *programa);

/* NULL si v sirve como cantidad de un repeat; si no, el motivo */
const char *problema_repeticiones(double v);

#endif
