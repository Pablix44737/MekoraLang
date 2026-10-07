#include "codegen.h"

void emitir_prologo(FILE *out)
{
    fprintf(out, "; Generado automaticamente - no editar\n");
    fprintf(out, "G21\n");   /* unidades en milimetros */
    fprintf(out, "G90\n");   /* posicionamiento absoluto (X, Y, Z) */
    fprintf(out, "M83\n");   /* extrusion relativa (E) */
}

void emitir_epilogo(FILE *out)
{
    fprintf(out, "M104 S0\n");   /* apagar extrusor */
    fprintf(out, "M140 S0\n");   /* apagar cama */
    fprintf(out, "M107\n");      /* apagar ventilador */
    fprintf(out, "M84\n");       /* deshabilitar motores */
}
