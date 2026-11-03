#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ast.h"
#include "codegen.h"
#include "errores.h"
#include "posicion.h"
#include "recorrido.h"
#include "semantico.h"
#include "simbolos.h"
#include "tokens.h"

extern FILE *yyin;
extern char *yytext;
extern int   yylineno;
extern int   col_inicio;

int yylex(void);
int yyparse(void);

static void uso(const char *prog)
{
    fprintf(stderr, "uso: %s <entrada.gc> [-o <salida.gcode>] [--tokens | --ast | --tabla]\n", prog);
    fprintf(stderr, "  --tokens   vuelca la lista de tokens y termina\n");
    fprintf(stderr, "  --ast      imprime el arbol sintactico y termina\n");
    fprintf(stderr, "  --tabla    hace el analisis semantico, imprime la tabla de simbolos y termina\n");
}

static void liberar_todo(void)
{
    ast_liberar(arbol);
    recorrido_liberar();
    simbolos_liberar();
}

/* Modo de prueba del analizador lexico: recorre la entrada token por token
 * y los imprime. No interviene el parser. */
static int volcar_tokens(void)
{
    int token;

    printf("%-5s %-5s %-14s %s\n", "LIN", "COL", "TOKEN", "LEXEMA");
    while ((token = yylex()) != 0) {
        printf("%-5d %-5d %-14s %s\n",
               yylineno, col_inicio, nombre_token(token), yytext);
    }

    if (errores > 0) {
        fprintf(stderr, "Analisis lexico fallido: %d error(es).\n", errores);
        return 1;
    }
    return 0;
}

int main(int argc, char **argv)
{
    const char *ruta_salida = "salida.gcode";
    int modo_tokens = 0;
    int modo_ast    = 0;
    int modo_tabla  = 0;
    FILE *salida;
    int i;

    if (argc < 2) {
        uso(argv[0]);
        return 2;
    }

    archivo_entrada = argv[1];

    for (i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--tokens") == 0) {
            modo_tokens = 1;
        } else if (strcmp(argv[i], "--ast") == 0) {
            modo_ast = 1;
        } else if (strcmp(argv[i], "--tabla") == 0) {
            modo_tabla = 1;
        } else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            ruta_salida = argv[++i];
        } else {
            fprintf(stderr, "opcion desconocida: %s\n", argv[i]);
            uso(argv[0]);
            return 2;
        }
    }

    yyin = fopen(archivo_entrada, "r");
    if (!yyin) {
        perror(archivo_entrada);
        return 2;
    }

    if (modo_tokens) {
        int r = volcar_tokens();
        fclose(yyin);
        return r;
    }

    /* Fase de analisis: lexico y sintactico juntos. Si termina bien, en
     * "arbol" queda el programa completo. */
    if (yyparse() != 0 && errores == 0)
        errores++;   /* no deberia pasar: todo error ya se reporta solo */
    fclose(yyin);

    /* --ast muestra el arbol tal como lo armo el parser, sin analisis
     * semantico: asi se puede ver tambien el de un programa que tiene
     * errores semanticos. */
    if (errores == 0 && modo_ast) {
        ast_imprimir(stdout, arbol);
        liberar_todo();
        return 0;
    }

    /* Analisis semantico, en dos recorridos. El primero arma la tabla de
     * simbolos y verifica nombres y tipos; el segundo recorre el programa
     * desde main, entrando en cada llamada, y verifica la posicion
     * conocida y los limites. El segundo necesita el arbol decorado por el
     * primero, asi que solo corre si el primero termino sin errores. */
    if (errores == 0) {
        analizar_semantica(arbol);
        if (errores == 0)
            analizar_posicion(arbol);
    }

    if (modo_tabla)
        simbolos_imprimir(stdout);

    if (errores > 0) {
        fprintf(stderr, "Compilacion fallida: %d error(es).\n", errores);
        liberar_todo();
        return 1;
    }

    if (modo_tabla) {
        liberar_todo();
        return 0;
    }

    /* Generacion. El archivo de salida se abre recien aca, cuando ya se
     * sabe que el fuente es valido. */
    salida = fopen(ruta_salida, "w");
    if (!salida) {
        perror(ruta_salida);
        liberar_todo();
        return 2;
    }

    emitir_prologo(salida);
    generar(arbol, salida);
    emitir_epilogo(salida);
    fclose(salida);

    /* La generacion conoce valores que el analisis no: puede encontrar un
     * error. Ese G-code no se le puede dar a la maquina, asi que no queda. */
    if (errores > 0) {
        remove(ruta_salida);
        fprintf(stderr, "Compilacion fallida: %d error(es).\n", errores);
        liberar_todo();
        return 1;
    }

    liberar_todo();

    if (advertencias > 0)
        printf("Compilacion exitosa: %s (%d advertencia%s)\n", ruta_salida,
               advertencias, advertencias == 1 ? "" : "s");
    else
        printf("Compilacion exitosa: %s\n", ruta_salida);
    return 0;
}
