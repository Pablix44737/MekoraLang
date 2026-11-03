#include <stdio.h>
#include <stdarg.h>
#include "errores.h"

int         errores         = 0;
int         advertencias    = 0;
const char *archivo_entrada = "<entrada>";

/* "clase" es lo que va despues de la posicion: "error lexico",
 * "advertencia", "nota". "contador" es el que se incrementa, o NULL. */
static void reportar(const char *clase, int *contador, int linea, int columna,
                     const char *fmt, va_list args)
{
    fprintf(stderr, "%s:%d:%d: %s: ", archivo_entrada, linea, columna, clase);
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    if (contador)
        (*contador)++;
}

void error_lexico(int linea, int columna, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    reportar("error lexico", &errores, linea, columna, fmt, args);
    va_end(args);
}

void error_sintactico(int linea, int columna, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    reportar("error sintactico", &errores, linea, columna, fmt, args);
    va_end(args);
}

void error_semantico(int linea, int columna, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    reportar("error semantico", &errores, linea, columna, fmt, args);
    va_end(args);
}

void error_generacion(int linea, int columna, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    reportar("error generacion", &errores, linea, columna, fmt, args);
    va_end(args);
}

void advertencia(int linea, int columna, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    reportar("advertencia", &advertencias, linea, columna, fmt, args);
    va_end(args);
}

void nota(int linea, int columna, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    reportar("nota", NULL, linea, columna, fmt, args);
    va_end(args);
}
