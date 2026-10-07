#ifndef CODEGEN_H
#define CODEGEN_H

#include <stdio.h>

/* Prologo y epilogo fijos. El programador del lenguaje fuente no los
 * escribe: el compilador los emite siempre.
 * Ver especificacion, seccion 6. */

void emitir_prologo(FILE *out);
void emitir_epilogo(FILE *out);

#endif
