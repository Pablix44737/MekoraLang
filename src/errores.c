#include <stdio.h>
#include <stdarg.h>
#include "errores.h"

int         errores         = 0;
const char *archivo_entrada = "<entrada>";

static void reportar(const char *fase, int linea, int columna,
                     const char *fmt, va_list args)
{
    fprintf(stderr, "%s:%d:%d: error %s: ",
            archivo_entrada, linea, columna, fase);
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    errores++;
}

void error_lexico(int linea, int columna, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    reportar("lexico", linea, columna, fmt, args);
    va_end(args);
}

void error_sintactico(int linea, int columna, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    reportar("sintactico", linea, columna, fmt, args);
    va_end(args);
}

void error_semantico(int linea, int columna, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    reportar("semantico", linea, columna, fmt, args);
    va_end(args);
}
