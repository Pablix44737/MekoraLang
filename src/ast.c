#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include "ast.h"

/* --- Construccion --- */

static Nodo *nodo_nuevo(TipoNodo tipo, int linea, int columna)
{
    Nodo *n = calloc(1, sizeof(Nodo));
    if (!n) {
        fprintf(stderr, "sin memoria para el arbol sintactico\n");
        exit(2);
    }
    n->tipo    = tipo;
    n->linea   = linea;
    n->columna = columna;
    return n;
}

Nodo *ast_programa(Nodo *declaraciones, Nodo *principal)
{
    Nodo *n = nodo_nuevo(N_PROGRAMA, 1, 1);
    n->programa.declaraciones = declaraciones;
    n->programa.principal     = principal;
    return n;
}

Nodo *ast_maquina(char *nombre, Nodo *campos, int linea, int columna)
{
    Nodo *n = nodo_nuevo(N_MAQUINA, linea, columna);
    n->maquina.nombre = nombre;
    n->maquina.campos = campos;
    return n;
}

Nodo *ast_campo(char *nombre, Nodo *valor, int linea, int columna)
{
    Nodo *n = nodo_nuevo(N_CAMPO, linea, columna);
    n->campo.nombre = nombre;
    n->campo.valor  = valor;
    return n;
}

Nodo *ast_const(char *nombre, TipoDato tipo, Nodo *valor, int linea, int columna)
{
    Nodo *n = nodo_nuevo(N_CONST, linea, columna);
    n->decl.nombre = nombre;
    n->decl.tipo   = tipo;
    n->decl.valor  = valor;
    return n;
}

Nodo *ast_procedimiento(char *nombre, Nodo *params, Nodo *cuerpo, int linea, int columna)
{
    Nodo *n = nodo_nuevo(N_PROCEDIMIENTO, linea, columna);
    n->proc.nombre = nombre;
    n->proc.params = params;
    n->proc.cuerpo = cuerpo;
    return n;
}

Nodo *ast_param(char *nombre, TipoDato tipo, int linea, int columna)
{
    Nodo *n = nodo_nuevo(N_PARAM, linea, columna);
    n->param.nombre = nombre;
    n->param.tipo   = tipo;
    return n;
}

Nodo *ast_bloque(Nodo *sentencias, int linea, int columna)
{
    Nodo *n = nodo_nuevo(N_BLOQUE, linea, columna);
    n->bloque.sentencias = sentencias;
    return n;
}

Nodo *ast_var(char *nombre, TipoDato tipo, Nodo *valor, int linea, int columna)
{
    Nodo *n = nodo_nuevo(N_VAR, linea, columna);
    n->decl.nombre = nombre;
    n->decl.tipo   = tipo;
    n->decl.valor  = valor;
    return n;
}

Nodo *ast_asignacion(char *nombre, Nodo *valor, int linea, int columna)
{
    Nodo *n = nodo_nuevo(N_ASIGNACION, linea, columna);
    n->asignacion.nombre = nombre;
    n->asignacion.valor  = valor;
    return n;
}

Nodo *ast_if(Nodo *cond, Nodo *entonces, Nodo *sino, int linea, int columna)
{
    Nodo *n = nodo_nuevo(N_IF, linea, columna);
    n->si.cond     = cond;
    n->si.entonces = entonces;
    n->si.sino     = sino;
    return n;
}

Nodo *ast_repeat(Nodo *veces, char *indice, Nodo *cuerpo, int linea, int columna)
{
    Nodo *n = nodo_nuevo(N_REPEAT, linea, columna);
    n->repetir.veces  = veces;
    n->repetir.indice = indice;
    n->repetir.cuerpo = cuerpo;
    return n;
}

Nodo *ast_llamada(char *nombre, Nodo *args, int linea, int columna)
{
    Nodo *n = nodo_nuevo(N_LLAMADA, linea, columna);
    n->llamada.nombre = nombre;
    n->llamada.args   = args;
    return n;
}

Nodo *ast_arg_eje(char eje, Nodo *valor, int linea, int columna)
{
    Nodo *n = nodo_nuevo(N_ARG_EJE, linea, columna);
    n->arg_eje.eje   = eje;
    n->arg_eje.valor = valor;
    return n;
}

Nodo *ast_cadena(char *texto, int linea, int columna)
{
    Nodo *n = nodo_nuevo(N_CADENA, linea, columna);
    n->cadena = texto;
    return n;
}

Nodo *ast_numero(double valor, int linea, int columna)
{
    Nodo *n = nodo_nuevo(N_NUMERO, linea, columna);
    n->numero = valor;
    return n;
}

Nodo *ast_bool(int valor, int linea, int columna)
{
    Nodo *n = nodo_nuevo(N_BOOL, linea, columna);
    n->booleano = valor;
    return n;
}

Nodo *ast_id(char *nombre, int linea, int columna)
{
    Nodo *n = nodo_nuevo(N_ID, linea, columna);
    n->id = nombre;
    return n;
}

Nodo *ast_acceso(char *objeto, char *campo, int linea, int columna)
{
    Nodo *n = nodo_nuevo(N_ACCESO, linea, columna);
    n->acceso.objeto = objeto;
    n->acceso.campo  = campo;
    return n;
}

Nodo *ast_binaria(Operador op, Nodo *izq, Nodo *der, int linea, int columna)
{
    Nodo *n = nodo_nuevo(N_BINARIA, linea, columna);
    n->binaria.op  = op;
    n->binaria.izq = izq;
    n->binaria.der = der;
    return n;
}

Nodo *ast_unaria(Operador op, Nodo *operando, int linea, int columna)
{
    Nodo *n = nodo_nuevo(N_UNARIA, linea, columna);
    n->unaria.op       = op;
    n->unaria.operando = operando;
    return n;
}

Nodo *ast_agregar(Nodo *lista, Nodo *nuevo)
{
    Nodo *ultimo;

    if (!nuevo)
        return lista;
    if (!lista)
        return nuevo;

    /* recorre hasta el final: las listas son cortas */
    for (ultimo = lista; ultimo->sig; ultimo = ultimo->sig)
        ;
    ultimo->sig = nuevo;
    return lista;
}

/* --- Nombres legibles --- */

const char *ast_nombre_tipo(TipoDato tipo)
{
    switch (tipo) {
    case DATO_NUMBER:  return "number";
    case DATO_BOOL:    return "bool";
    case DATO_MACHINE: return "machine";
    case DATO_ERROR:   return "<error>";
    }
    return "?";
}

const char *ast_nombre_operador(Operador op)
{
    switch (op) {
    case OP_SUMA:        return "+";
    case OP_RESTA:       return "-";
    case OP_MULT:        return "*";
    case OP_DIV:         return "/";
    case OP_MOD:         return "%";
    case OP_IGUAL:       return "==";
    case OP_DISTINTO:    return "!=";
    case OP_MENOR:       return "<";
    case OP_MAYOR:       return ">";
    case OP_MENOR_IGUAL: return "<=";
    case OP_MAYOR_IGUAL: return ">=";
    case OP_AND:         return "and";
    case OP_OR:          return "or";
    case OP_NOT:         return "not";
    case OP_NEG:         return "-";
    }
    return "?";
}

/* --- Impresion --- */

/* Una linea del arbol: sangria, rol (cond, entonces...), descripcion y
 * posicion. */
static void renglon(FILE *out, int nivel, const char *rol, const Nodo *n,
                    const char *fmt, ...)
{
    va_list args;
    int i;

    for (i = 0; i < nivel; i++)
        fputs("  ", out);
    if (rol)
        fprintf(out, "%s: ", rol);

    va_start(args, fmt);
    vfprintf(out, fmt, args);
    va_end(args);

    fprintf(out, "  (%d:%d)\n", n->linea, n->columna);
}

static void imprimir(FILE *out, const Nodo *n, int nivel, const char *rol);

static void imprimir_lista(FILE *out, const Nodo *n, int nivel)
{
    for (; n; n = n->sig)
        imprimir(out, n, nivel, NULL);
}

static void imprimir(FILE *out, const Nodo *n, int nivel, const char *rol)
{
    char eje[2];

    if (!n)
        return;

    switch (n->tipo) {
    case N_PROGRAMA:
        renglon(out, nivel, rol, n, "programa");
        imprimir_lista(out, n->programa.declaraciones, nivel + 1);
        imprimir(out, n->programa.principal, nivel + 1, "main");
        break;

    case N_MAQUINA:
        renglon(out, nivel, rol, n, "machine %s", n->maquina.nombre);
        imprimir_lista(out, n->maquina.campos, nivel + 1);
        break;

    case N_CAMPO:
        renglon(out, nivel, rol, n, "campo %s", n->campo.nombre);
        imprimir(out, n->campo.valor, nivel + 1, NULL);
        break;

    case N_CONST:
    case N_VAR:
        renglon(out, nivel, rol, n, "%s %s : %s",
                n->tipo == N_CONST ? "const" : "var",
                n->decl.nombre, ast_nombre_tipo(n->decl.tipo));
        imprimir(out, n->decl.valor, nivel + 1, NULL);
        break;

    case N_PROCEDIMIENTO:
        renglon(out, nivel, rol, n, "procedure %s", n->proc.nombre);
        imprimir_lista(out, n->proc.params, nivel + 1);
        imprimir(out, n->proc.cuerpo, nivel + 1, NULL);
        break;

    case N_PARAM:
        renglon(out, nivel, rol, n, "param %s : %s",
                n->param.nombre, ast_nombre_tipo(n->param.tipo));
        break;

    case N_BLOQUE:
        renglon(out, nivel, rol, n, "bloque");
        imprimir_lista(out, n->bloque.sentencias, nivel + 1);
        break;

    case N_ASIGNACION:
        renglon(out, nivel, rol, n, "asignacion %s", n->asignacion.nombre);
        imprimir(out, n->asignacion.valor, nivel + 1, NULL);
        break;

    case N_IF:
        renglon(out, nivel, rol, n, "if");
        imprimir(out, n->si.cond,     nivel + 1, "cond");
        imprimir(out, n->si.entonces, nivel + 1, "entonces");
        imprimir(out, n->si.sino,     nivel + 1, "sino");
        break;

    case N_REPEAT:
        if (n->repetir.indice)
            renglon(out, nivel, rol, n, "repeat as %s", n->repetir.indice);
        else
            renglon(out, nivel, rol, n, "repeat");
        imprimir(out, n->repetir.veces,  nivel + 1, "veces");
        imprimir(out, n->repetir.cuerpo, nivel + 1, "cuerpo");
        break;

    case N_LLAMADA:
        renglon(out, nivel, rol, n, "llamada %s", n->llamada.nombre);
        imprimir_lista(out, n->llamada.args, nivel + 1);
        break;

    case N_ARG_EJE:
        /* eje con valor: se imprime como "X: 10" */
        eje[0] = n->arg_eje.eje;
        eje[1] = '\0';
        if (n->arg_eje.valor)
            imprimir(out, n->arg_eje.valor, nivel, eje);
        else
            renglon(out, nivel, rol, n, "eje %s", eje);
        break;

    case N_CADENA:
        renglon(out, nivel, rol, n, "cadena \"%s\"", n->cadena);
        break;

    case N_NUMERO:
        renglon(out, nivel, rol, n, "numero %g", n->numero);
        break;

    case N_BOOL:
        renglon(out, nivel, rol, n, "bool %s", n->booleano ? "true" : "false");
        break;

    case N_ID:
        renglon(out, nivel, rol, n, "id %s", n->id);
        break;

    case N_ACCESO:
        renglon(out, nivel, rol, n, "acceso %s.%s",
                n->acceso.objeto, n->acceso.campo);
        break;

    case N_BINARIA:
        renglon(out, nivel, rol, n, "binaria %s",
                ast_nombre_operador(n->binaria.op));
        imprimir(out, n->binaria.izq, nivel + 1, NULL);
        imprimir(out, n->binaria.der, nivel + 1, NULL);
        break;

    case N_UNARIA:
        renglon(out, nivel, rol, n, "unaria %s",
                ast_nombre_operador(n->unaria.op));
        imprimir(out, n->unaria.operando, nivel + 1, NULL);
        break;
    }
}

void ast_imprimir(FILE *out, const Nodo *raiz)
{
    imprimir(out, raiz, 0, NULL);
}

/* --- Liberacion --- */

void ast_liberar(Nodo *n)
{
    while (n) {
        Nodo *sig = n->sig;

        switch (n->tipo) {
        case N_PROGRAMA:
            ast_liberar(n->programa.declaraciones);
            ast_liberar(n->programa.principal);
            break;
        case N_MAQUINA:
            free(n->maquina.nombre);
            ast_liberar(n->maquina.campos);
            break;
        case N_CAMPO:
            free(n->campo.nombre);
            ast_liberar(n->campo.valor);
            break;
        case N_CONST:
        case N_VAR:
            free(n->decl.nombre);
            ast_liberar(n->decl.valor);
            break;
        case N_PROCEDIMIENTO:
            free(n->proc.nombre);
            ast_liberar(n->proc.params);
            ast_liberar(n->proc.cuerpo);
            break;
        case N_PARAM:
            free(n->param.nombre);
            break;
        case N_BLOQUE:
            ast_liberar(n->bloque.sentencias);
            break;
        case N_ASIGNACION:
            free(n->asignacion.nombre);
            ast_liberar(n->asignacion.valor);
            break;
        case N_IF:
            ast_liberar(n->si.cond);
            ast_liberar(n->si.entonces);
            ast_liberar(n->si.sino);
            break;
        case N_REPEAT:
            ast_liberar(n->repetir.veces);
            free(n->repetir.indice);
            ast_liberar(n->repetir.cuerpo);
            break;
        case N_LLAMADA:
            free(n->llamada.nombre);
            ast_liberar(n->llamada.args);
            break;
        case N_ARG_EJE:
            ast_liberar(n->arg_eje.valor);
            break;
        case N_CADENA:
            free(n->cadena);
            break;
        case N_ID:
            free(n->id);
            break;
        case N_ACCESO:
            free(n->acceso.objeto);
            free(n->acceso.campo);
            break;
        case N_BINARIA:
            ast_liberar(n->binaria.izq);
            ast_liberar(n->binaria.der);
            break;
        case N_UNARIA:
            ast_liberar(n->unaria.operando);
            break;
        case N_NUMERO:
        case N_BOOL:
            break;
        }

        free(n);
        n = sig;
    }
}
