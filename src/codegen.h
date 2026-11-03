#ifndef CODEGEN_H
#define CODEGEN_H

#include <stdio.h>
#include "ast.h"

/* Prologo y epilogo fijos. El programador del lenguaje fuente no los
 * escribe: el compilador los emite siempre.
 * Ver especificacion, seccion 6. */

void emitir_prologo(FILE *out);
void emitir_epilogo(FILE *out);

/* Recorre el programa desde main y emite su G-code. Necesita el arbol
 * decorado por el analisis semantico, y que el analisis no haya dado
 * errores. Los errores que aparecen aca (un valor que recien se conoce al
 * generar) cuentan en "errores". */
void generar(Nodo *programa, FILE *out);

#endif
