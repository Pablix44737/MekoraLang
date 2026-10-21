#ifndef ERRORES_H
#define ERRORES_H

/* Reporte centralizado de errores.
 *
 * Todos los errores salen con el formato
 *
 *     archivo:linea:columna: error <fase>: <mensaje>
 *
 * que es el mismo que usa gcc. Centralizarlo en un solo lugar evita que
 * cada fase invente su propio formato.
 *
 * Ademas de los errores hay advertencias y notas, con el mismo formato
 * ("advertencia:" y "nota:" en lugar de "error <fase>:"). Ninguna de las
 * dos cuenta como error ni hace fallar la compilacion. La nota sirve para
 * agregar contexto a un error, por ejemplo desde que llamada se llego al
 * procedimiento donde ocurrio. */

extern int         errores;           /* contador global */
extern int         advertencias;      /* aparte: no hacen fallar */
extern const char *archivo_entrada;   /* nombre del archivo fuente */

void error_lexico   (int linea, int columna, const char *fmt, ...);
void error_sintactico(int linea, int columna, const char *fmt, ...);
void error_semantico (int linea, int columna, const char *fmt, ...);

void advertencia(int linea, int columna, const char *fmt, ...);
void nota       (int linea, int columna, const char *fmt, ...);

#endif
