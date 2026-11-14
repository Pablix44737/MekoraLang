#ifndef CODEGEN_H
#define CODEGEN_H

#include <stdio.h>
#include "ast.h"

/* Prologo y epilogo fijos */

void emitir_prologo(FILE *out);
void emitir_epilogo(FILE *out);

/* Emite el G-code desde main. Solo si el analisis no dio errores. */
void generar(Nodo *programa, FILE *out);

#endif
