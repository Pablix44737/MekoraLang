#ifndef SEMANTICO_H
#define SEMANTICO_H

#include "ast.h"

/* semantico.h - Analisis semantico, primer recorrido
 *
 * Recorre las declaraciones en el orden del fuente y despues main. Arma la
 * tabla de simbolos (simbolos.h), resuelve cada nombre con la regla del
 * bloque anidado mas cercano, calcula el tipo de cada expresion a partir
 * de los tipos de sus partes y valida la forma de cada llamada. Deja el
 * arbol decorado: cada uso de un nombre apunta a su entrada en la tabla.
 *
 * El segundo recorrido (posicion.h) solo corre si este termino sin
 * errores. Ver especificacion, secciones 5 y 8. */

/* Campos que toda maquina tiene que declarar. Son los unicos nombres de
 * campo con significado para el compilador (especificacion, seccion 5.1). */
#define CAMPO_LIMITE_X      "limite_x"
#define CAMPO_LIMITE_Y      "limite_y"
#define CAMPO_LIMITE_Z      "limite_z"
#define CAMPO_FEEDRATE_MAX  "feedrate_max"

void analizar_semantica(Nodo *programa);

/* NULL si "v" sirve como cantidad de repeticiones de un repeat; si no, el
 * motivo, para completar "la cantidad de repeticiones ...". Lo usan los
 * dos recorridos. */
const char *problema_repeticiones(double v);

#endif
