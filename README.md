# MekoraLang

Compilador del lenguaje MekoraLang, que traduce codigo de alto nivel a G-code
para impresoras 3D. TP final de Compiladores, Ingenieria en Informatica.

## Dependencias

flex, bison, gcc, make.

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

## Probar

    make test      compila tests/minimo.gc y muestra el G-code generado
    make tokens    vuelca los tokens de tests/ejemplo.gc

## Estructura

    src/lexer.l      analizador lexico (flex)
    src/parser.y     analizador sintactico (bison), declaracion de tokens
    src/tokens.c     nombres de los tokens, para el modo --tokens
    src/errores.c    reporte de errores con archivo, linea y columna
    src/codegen.c    emision de G-code
    src/main.c       punto de entrada, argumentos y codigo de salida
    tests/           programas de prueba, validos y con errores
    build/           archivos generados, no se versiona

## Estado

Analizador lexico completo. Reconoce las palabras reservadas, los ejes, los
operadores, numeros, identificadores y cadenas, con seguimiento de linea y
columna, y reporta caracteres no reconocidos y cadenas sin cerrar.

El parser tiene todos los tokens declarados pero la gramatica reconoce solo
`main { }`. Las producciones del lenguaje completo son el paso siguiente, asi
que por ahora un programa real se prueba con `--tokens`, no compilandolo.
