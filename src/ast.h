#ifndef AST_H
#define AST_H

#include <stdio.h>

/* ast.h - Arbol de sintaxis abstracta
 *
 * Un solo tipo Nodo: "tipo" dice la clase y la union guarda sus datos
 * (N_IF usa n->si, N_LLAMADA usa n->llamada...). Las listas se encadenan
 * con "sig". */

/* DATO_ERROR: tipo de una expresion mal formada (evita errores en cascada) */
typedef enum {
    DATO_NUMBER,
    DATO_BOOL,
    DATO_MACHINE,
    DATO_ERROR
} TipoDato;

typedef enum {
    OP_SUMA, OP_RESTA, OP_MULT, OP_DIV, OP_MOD,
    OP_IGUAL, OP_DISTINTO, OP_MENOR, OP_MAYOR, OP_MENOR_IGUAL, OP_MAYOR_IGUAL,
    OP_AND, OP_OR,
    OP_NOT, OP_NEG                  /* unarios */
} Operador;

typedef enum {
    /* Declaraciones */
    N_PROGRAMA,
    N_MAQUINA,          /* machine Ender3 { ... }                */
    N_CAMPO,            /* temp_cama: 60;   (dentro de machine)  */
    N_CONST,            /* const MARGEN : number = 10;           */
    N_PROCEDIMIENTO,    /* procedure Preparar(m: machine) { }    */
    N_PARAM,            /* m: machine                            */

    /* Sentencias */
    N_BLOQUE,           /* { ... }                               */
    N_VAR,              /* var vel : number = 1500;              */
    N_ASIGNACION,       /* vel = 3000;                           */
    N_IF,
    N_REPEAT,
    N_LLAMADA,          /* Move(X: 10);   Preparar(Ender3);      */

    /* Argumentos que no son expresiones */
    N_ARG_EJE,          /* X: 10, o el eje solo: X               */
    N_CADENA,           /* "texto"                               */

    /* Expresiones */
    N_NUMERO,
    N_BOOL,
    N_ID,               /* MARGEN, vel, Ender3                   */
    N_ACCESO,           /* m.limite_x                            */
    N_BINARIA,
    N_UNARIA
} TipoNodo;

typedef struct Nodo Nodo;
struct Simbolo;

struct Nodo {
    TipoNodo tipo;
    int      linea;
    int      columna;
    Nodo    *sig;       /* siguiente elemento, si el nodo esta en una lista */

    /* Entrada de la tabla de simbolos del nombre usado o declarado. La
     * carga el primer recorrido; el parser la deja en NULL. */
    struct Simbolo *simbolo;

    union {
        struct { Nodo *declaraciones; Nodo *principal; }      programa;
        struct { char *nombre; Nodo *campos; }                maquina;
        struct { char *nombre; Nodo *valor; }                 campo;
        struct { char *nombre; TipoDato tipo; Nodo *valor; }  decl;       /* N_CONST y N_VAR */
        struct { char *nombre; Nodo *params; Nodo *cuerpo; }  proc;
        struct { char *nombre; TipoDato tipo; }               param;
        struct { Nodo *sentencias; }                          bloque;
        struct { char *nombre; Nodo *valor; }                 asignacion;
        struct { Nodo *cond; Nodo *entonces; Nodo *sino; }    si;         /* sino: NULL, N_BLOQUE o N_IF */
        struct { Nodo *veces; char *indice; Nodo *cuerpo; }   repetir;    /* indice: NULL si no hay "as" */
        struct { char *nombre; Nodo *args; }                  llamada;
        struct { char eje; Nodo *valor; }                     arg_eje;    /* valor: NULL si va el eje solo */
        char    *cadena;
        double   numero;
        int      booleano;
        char    *id;
        struct { char *objeto; char *campo; }                 acceso;
        struct { Operador op; Nodo *izq; Nodo *der; }         binaria;
        struct { Operador op; Nodo *operando; }               unaria;
    };
};

/* Raiz del arbol, la carga parser.y */
extern Nodo *arbol;

/* Constructores. Los char* vienen del lexer (strdup) y no se copian. */
Nodo *ast_programa     (Nodo *declaraciones, Nodo *principal);
Nodo *ast_maquina      (char *nombre, Nodo *campos, int linea, int columna);
Nodo *ast_campo        (char *nombre, Nodo *valor, int linea, int columna);
Nodo *ast_const        (char *nombre, TipoDato tipo, Nodo *valor, int linea, int columna);
Nodo *ast_procedimiento(char *nombre, Nodo *params, Nodo *cuerpo, int linea, int columna);
Nodo *ast_param        (char *nombre, TipoDato tipo, int linea, int columna);

Nodo *ast_bloque       (Nodo *sentencias, int linea, int columna);
Nodo *ast_var          (char *nombre, TipoDato tipo, Nodo *valor, int linea, int columna);
Nodo *ast_asignacion   (char *nombre, Nodo *valor, int linea, int columna);
Nodo *ast_if           (Nodo *cond, Nodo *entonces, Nodo *sino, int linea, int columna);
Nodo *ast_repeat       (Nodo *veces, char *indice, Nodo *cuerpo, int linea, int columna);
Nodo *ast_llamada      (char *nombre, Nodo *args, int linea, int columna);

Nodo *ast_arg_eje      (char eje, Nodo *valor, int linea, int columna);
Nodo *ast_cadena       (char *texto, int linea, int columna);

Nodo *ast_numero       (double valor, int linea, int columna);
Nodo *ast_bool         (int valor, int linea, int columna);
Nodo *ast_id           (char *nombre, int linea, int columna);
Nodo *ast_acceso       (char *objeto, char *campo, int linea, int columna);
Nodo *ast_binaria      (Operador op, Nodo *izq, Nodo *der, int linea, int columna);
Nodo *ast_unaria       (Operador op, Nodo *operando, int linea, int columna);

/* Agrega al final. Si nuevo es NULL (sentencia descartada) no hace nada. */
Nodo *ast_agregar(Nodo *lista, Nodo *nuevo);

/* Modo --ast: un nodo por linea con su posicion */
void ast_imprimir(FILE *out, const Nodo *raiz);

/* Libera el nodo, sus hijos y el resto de la lista que cuelga de "sig". */
void ast_liberar(Nodo *n);

const char *ast_nombre_tipo(TipoDato tipo);
const char *ast_nombre_operador(Operador op);

#endif
