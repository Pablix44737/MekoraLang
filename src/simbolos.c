#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "simbolos.h"

/* Todos los alcances creados, en orden, para imprimirlos y liberarlos. */
static Alcance *primero = NULL;
static Alcance *ultimo  = NULL;

static void *reservar(size_t tam)
{
    void *p = calloc(1, tam);
    if (!p) {
        fprintf(stderr, "sin memoria para la tabla de simbolos\n");
        exit(2);
    }
    return p;
}

Alcance *alcance_abrir(Alcance *anterior, int linea, const char *fmt, ...)
{
    Alcance *a = reservar(sizeof(Alcance));
    va_list args;

    va_start(args, fmt);
    vsnprintf(a->descripcion, sizeof a->descripcion, fmt, args);
    va_end(args);

    a->linea    = linea;
    a->anterior = anterior;
    a->nivel    = anterior ? anterior->nivel + 1 : 0;

    if (ultimo)
        ultimo->sig_todos = a;
    else
        primero = a;
    ultimo = a;
    return a;
}

Simbolo *alcance_insertar(Alcance *a, const char *nombre, ClaseSimbolo clase,
                          TipoDato tipo, Nodo *decl)
{
    Simbolo *s = reservar(sizeof(Simbolo));

    s->nombre = nombre;
    s->clase  = clase;
    s->tipo   = tipo;
    s->decl   = decl;

    if (a->ultimo)
        a->ultimo->sig = s;
    else
        a->simbolos = s;
    a->ultimo = s;

    if (decl)
        decl->simbolo = s;
    return s;
}

Simbolo *alcance_buscar_local(const Alcance *a, const char *nombre)
{
    Simbolo *s;

    for (s = a->simbolos; s; s = s->sig)
        if (strcmp(s->nombre, nombre) == 0)
            return s;
    return NULL;
}

Simbolo *alcance_buscar(const Alcance *a, const char *nombre)
{
    Simbolo *s;

    for (; a; a = a->anterior) {
        s = alcance_buscar_local(a, nombre);
        if (s)
            return s;
    }
    return NULL;
}

const char *simbolos_nombre_clase(ClaseSimbolo clase)
{
    switch (clase) {
    case S_BUILTIN: return "builtin";
    case S_MAQUINA: return "machine";
    case S_CAMPO:   return "campo";
    case S_CONST:   return "const";
    case S_PROC:    return "procedure";
    case S_PARAM:   return "param";
    case S_VAR:     return "var";
    case S_INDICE:  return "indice";
    }
    return "?";
}

void simbolos_imprimir(FILE *out)
{
    Alcance *a;
    Simbolo *s;
    int i;

    for (a = primero; a; a = a->sig_todos) {
        for (i = 0; i < a->nivel; i++)
            fputs("  ", out);
        fprintf(out, "alcance %s", a->descripcion);
        if (a->linea > 0)
            fprintf(out, " (linea %d)", a->linea);
        if (a->anterior)
            fprintf(out, ", dentro de %s", a->anterior->descripcion);
        fputc('\n', out);

        /* Los builtins son muchos y siempre los mismos: van en una linea. */
        if (a->simbolos && a->simbolos->clase == S_BUILTIN) {
            for (i = 0; i <= a->nivel; i++)
                fputs("  ", out);
            for (s = a->simbolos; s; s = s->sig)
                fprintf(out, "%s%s", s->nombre, s->sig ? " " : "\n");
            continue;
        }

        for (s = a->simbolos; s; s = s->sig) {
            for (i = 0; i <= a->nivel; i++)
                fputs("  ", out);
            fprintf(out, "%-9s %s", simbolos_nombre_clase(s->clase), s->nombre);
            if (s->clase != S_PROC && s->clase != S_MAQUINA)
                fprintf(out, " : %s", ast_nombre_tipo(s->tipo));
            if ((s->clase == S_CONST || s->clase == S_CAMPO) && s->conocido) {
                if (s->tipo == DATO_BOOL)
                    fprintf(out, " = %s", s->valor != 0 ? "true" : "false");
                else
                    fprintf(out, " = %g", s->valor);
            }
            if (s->decl)
                fprintf(out, "  (linea %d)", s->decl->linea);
            fputc('\n', out);
        }
    }
}

void simbolos_liberar(void)
{
    Alcance *a = primero;

    while (a) {
        Alcance *sig_a = a->sig_todos;
        Simbolo *s = a->simbolos;

        while (s) {
            Simbolo *sig_s = s->sig;
            free(s);
            s = sig_s;
        }
        free(a);
        a = sig_a;
    }
    primero = ultimo = NULL;
}
