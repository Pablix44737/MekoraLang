#ifndef ERRORES_H
#define ERRORES_H

/* errores.h - Reporte de errores
 *
 * Formato (como gcc): archivo:linea:columna: error <fase>: mensaje
 * Advertencias y notas no cuentan como error. */

extern int         errores;           /* contador global */
extern int         advertencias;      /* aparte: no hacen fallar */
extern const char *archivo_entrada;   /* nombre del archivo fuente */

void error_lexico   (int linea, int columna, const char *fmt, ...);
void error_sintactico(int linea, int columna, const char *fmt, ...);
void error_semantico (int linea, int columna, const char *fmt, ...);
void error_generacion(int linea, int columna, const char *fmt, ...);

void advertencia(int linea, int columna, const char *fmt, ...);
void nota       (int linea, int columna, const char *fmt, ...);

#endif
