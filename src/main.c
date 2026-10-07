#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "errores.h"
#include "tokens.h"

extern FILE *yyin;
extern char *yytext;
extern int   yylineno;
extern int   col_inicio;

int yylex(void);
int yyparse(void);

/* Archivo de salida donde el generador escribe el G-code. */
FILE *salida = NULL;

static void uso(const char *prog)
{
    fprintf(stderr, "uso: %s <entrada.gc> [-o <salida.gcode>] [--tokens]\n", prog);
    fprintf(stderr, "  --tokens   vuelca la lista de tokens y termina\n");
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
    int i;

    if (argc < 2) {
        uso(argv[0]);
        return 2;
    }

    archivo_entrada = argv[1];

    for (i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--tokens") == 0) {
            modo_tokens = 1;
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

    salida = fopen(ruta_salida, "w");
    if (!salida) {
        perror(ruta_salida);
        fclose(yyin);
        return 2;
    }

    yyparse();

    fclose(salida);
    fclose(yyin);

    if (errores > 0) {
        fprintf(stderr, "Compilacion fallida: %d error(es).\n", errores);
        remove(ruta_salida);
        return 1;
    }

    printf("Compilacion exitosa: %s\n", ruta_salida);
    return 0;
}
