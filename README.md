# MekoraLang

Compilador del lenguaje MekoraLang, que traduce codigo de alto nivel a G-code
para impresoras 3D. TP final de Compiladores, Ingenieria en Informatica.

## Dependencias

flex, bison 3.6 o posterior, gcc, make. La version de bison se ve con
`bison --version`; la gramatica usa mensajes de error propios, que bison
soporta desde la 3.6.

En Debian/Ubuntu:

    sudo apt install flex bison build-essential

## Compilar

    make

Genera el ejecutable `gcodec` en la raiz del proyecto.

## Usar

    ./gcodec entrada.gc -o salida.gcode

Si se omite `-o`, la salida va a `salida.gcode`. El ejecutable devuelve 0 si
la compilacion fue exitosa y 1 si hubo errores.

Con `--tokens` se vuelca la lista de tokens y no interviene el parser. Sirve
para probar el analizador lexico aislado:

    ./gcodec entrada.gc --tokens

Con `--ast` se imprime el arbol sintactico indentado, un nodo por linea con
su posicion en el fuente:

    ./gcodec entrada.gc --ast

## Probar

    make test      compila tests/minimo.gc y muestra el G-code generado
    make tokens    vuelca los tokens de tests/ejemplo.gc
    make ast       imprime el arbol de tests/ejemplo.gc

`make tokens` y `make ast` aceptan otro programa:

    make ast PROG=tests/sintaxis.gc

## Estructura

    src/lexer.l      analizador lexico (flex)
    src/parser.y     analizador sintactico (bison) y construccion del arbol
    src/ast.c        arbol sintactico: nodos, impresion y liberacion
    src/tokens.c     nombres de los tokens, para el modo --tokens
    src/errores.c    reporte de errores con archivo, linea y columna
    src/codegen.c    emision de G-code
    src/main.c       punto de entrada, argumentos y codigo de salida
    tests/           programas de prueba, validos y con errores
    build/           archivos generados, no se versiona

## Estado

Analizador lexico y sintactico completos. El parser reconoce toda la
gramatica de la especificacion, sin conflictos, y construye el arbol
sintactico. Ante un error de sintaxis descarta hasta el proximo `;` y sigue,
asi que un mismo archivo puede reportar varios errores.

Falta el analisis semantico y la generacion del cuerpo del programa. Por
ahora un programa valido compila a solo el prologo y el epilogo; el arbol se
inspecciona con `--ast`.
