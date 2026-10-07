#ifndef ERRORES_H
#define ERRORES_H

/* Reporte centralizado de errores.
 *
 * Todos los errores salen con el formato
 *
 *     archivo:linea:columna: error <fase>: <mensaje>
 *
 * que es el mismo que usa gcc. Centralizarlo en un solo lugar evita que
 * cada fase invente su propio formato. */

extern int         errores;           /* contador global */
extern const char *archivo_entrada;   /* nombre del archivo fuente */

void error_lexico   (int linea, int columna, const char *fmt, ...);
void error_sintactico(int linea, int columna, const char *fmt, ...);
void error_semantico (int linea, int columna, const char *fmt, ...);

#endif
