# MekoraLang

MekoraLang es un lenguaje para escribir rutinas de mantenimiento, calibración
y piezas simples para una impresora 3D Ender 3. El compilador, `gcodec`, está
escrito en C con flex y bison y traduce los programas a G-code para firmware
Marlin. No es un slicer: no lee modelos 3D ni calcula soportes o relleno.

Trabajo final de Compiladores, Ingeniería en Informática, UGD.

El lenguaje (tipos, sentencias, builtins, reglas y gramática) está descrito
en `docs/referencia-lenguaje.md`.

## Compilar

Hace falta flex, bison 3.6 o posterior, gcc y make. Desde la raíz del
proyecto:

```
make
```

Deja el ejecutable `gcodec` en la raíz. Todo lo que generan flex, bison y gcc
va a `build/`. `make clean` borra todo lo generado.

## Probar todo de una vez

```
make pruebas
```

Corre los 25 programas de `tests/`, uno o dos por fase, y termina con:

```
25 pruebas: 25 pasan, 0 fallan
```

Cada archivo de `tests/` dice en su segunda línea qué se espera de él, por
ejemplo `// Esperado: error en la linea 13: 'margen' no esta declarado`. El
script `tests/correr_pruebas.sh` compila cada uno y verifica el código de
salida, la línea y el texto de cada error, las advertencias y, en los
programas válidos, que el G-code sea igual al de `tests/esperado/`. Los
archivos `err_*.gc` tienen un error a propósito; `gen_*.gc` y `err_gen_*.gc`
prueban la generación.

## Uso

```
./gcodec entrada.gc [-o salida.gcode] [--tokens | --ast | --tabla]
```

Sin `-o`, la salida va a `salida.gcode`. Devuelve 0 si compiló y 1 si hubo
errores; las advertencias no cambian el código de salida. Los errores tienen
el formato de gcc, `archivo:linea:columna: error <fase>: mensaje`, y ante un
error el compilador sigue para mostrar todos los que encuentre.

`--tokens` vuelca los tokens sin pasar por el parser, `--ast` imprime el árbol
sintáctico (sin análisis semántico) y `--tabla` hace el análisis semántico e
imprime la tabla de símbolos por alcance. Lo mismo se puede pedir desde make:
`make tokens`, `make ast` y `make tabla`, con `PROG=archivo.gc` para elegir el
programa.

## Escenarios

### Un programa completo

```
./gcodec tests/ejemplo_completo.gc -o ejemplo.gcode
```

Es un programa completo de ejemplo: prepara la máquina,
prueba los ejes, purga, barre la cama y dibuja un cuadrado. Compila con dos
advertencias, porque en `TestEjes` la velocidad `F` depende de una variable y
el análisis no la puede verificar. En `ejemplo.gcode` se ve el prólogo (`G21`,
`G90`, `M83`), las temperaturas tomadas de la máquina, los `repeat`
desenrollados, los procedimientos expandidos en el lugar de cada llamada, un
comentario antes de cada línea que tuvo advertencia y el epílogo que apaga la
máquina.

### Una pieza para ver en un simulador

```
./gcodec ejemplos/torre.gc -o torre.gcode
```

`ejemplos/torre.gc` imprime una diana de seis cuadrados concéntricos en la
primera capa y una torre hueca de 40 capas en el centro. La hicimos para
abrirla en un visor de G-code, porque los visores muestran los movimientos y
la extrusión, pero no las temperaturas, el ventilador ni el estado de los
motores, que es casi todo lo que hacen las rutinas de mantenimiento.

Recomendamos el visor de G-code de PrusaSlicer, que está para Linux como
AppImage y en Flathub. Se abre desde el menú File > G-code preview, o
arrastrando el archivo a la ventana. Muestra cada capa por separado,
distingue los desplazamientos de la extrusión y deja recorrer la impresión
movimiento por movimiento. Si no se quiere instalar nada, NC Viewer
(https://ncviewer.com) abre el archivo en el navegador, aunque está pensado
para CNC y no distingue tan bien la extrusión.

### Un error de cada fase

Léxico. El `@` no es un carácter del lenguaje; el segundo error es la cascada
en el parser:

```
$ ./gcodec tests/err_lexico.gc
tests/err_lexico.gc:2:24: error lexico: caracter no reconocido '@'
tests/err_lexico.gc:2:26: error sintactico: no se esperaba '3'
```

Sintáctico, con recuperación. El parser descarta hasta el próximo `;` y
sigue, así que reporta los tres errores del archivo:

```
$ ./gcodec tests/err_recuperacion.gc
tests/err_recuperacion.gc:4:25: error sintactico: no se esperaba ';'
tests/err_recuperacion.gc:6:26: error sintactico: no se esperaba '<'
tests/err_recuperacion.gc:7:12: error sintactico: se esperaba ',', ':' o ')' y se encontro '10'
```

Semántico, nombres y tipos (primer recorrido del árbol):

```
$ ./gcodec tests/err_no_declarado.gc
tests/err_no_declarado.gc:13:13: error semantico: 'margen' no esta declarado

$ ./gcodec tests/err_tipo_op.gc
tests/err_tipo_op.gc:12:24: error semantico: el operador '+' se aplica a dos number, y aca recibe number y bool
```

Semántico, posición conocida y límites (segundo recorrido, desde `main`). En
el primero, una rama del `if` libera los motores y la otra no; el análisis
une las dos ramas sin mirar la condición. En el segundo, el error aparece
recién en la quinta vuelta del `repeat`:

```
$ ./gcodec tests/err_pos_rama.gc
tests/err_pos_rama.gc:19:5: error semantico: 'Move' con posicion desconocida en el eje X: despues del if de la linea 16 la posicion solo es conocida en una de las ramas (la perdio DisableMotors() en la linea 17)

$ ./gcodec tests/err_limite_indice.gc
tests/err_limite_indice.gc:15:21: error semantico: Y = 250 esta fuera de los limites de la maquina: tiene que estar entre 0 y 220 (Ender3.limite_y) (con fila = 4)
```

Generación. El análisis no sigue el valor de las variables y solo puede
advertir; al generar ya se sabe que `x` vale 500, así que es un error y no
queda archivo de salida:

```
$ ./gcodec tests/err_gen_limite_variable.gc -o rechazo.gcode
tests/err_gen_limite_variable.gc:15:10: advertencia: no se puede verificar en compilacion que X quede entre 0 y 220: depende de la variable 'x'
tests/err_gen_limite_variable.gc:15:10: error generacion: X = 500 esta fuera de los limites de la maquina: tiene que estar entre 0 y 220 (Ender3.limite_x)
$ ls rechazo.gcode
ls: cannot access 'rechazo.gcode': No such file or directory
```

### Las fases por dentro

```
./gcodec tests/minimo.gc --tokens
./gcodec tests/precedencia.gc --ast
./gcodec tests/ocultamiento.gc --tabla
```

`precedencia.gc` sirve para ver en el árbol cómo se agrupan los operadores, y
`ocultamiento.gc` para ver en la tabla los alcances anidados y un nombre que
oculta a otro.

## Editor

En `vscode-mekoralang/` hay una extensión de VS Code que colorea los `.gc`:
palabras reservadas, builtins, ejes, números, cadenas y comentarios. El
paquete ya está armado:

```
code --install-extension vscode-mekoralang/mekoralang-0.1.0.vsix
```

## Estructura

```
src/lexer.l       analizador lexico (flex)
src/parser.y      analizador sintactico (bison) y construccion del arbol
src/ast.c         arbol sintactico
src/tokens.c      nombres de los tokens, para --tokens
src/simbolos.c    tabla de simbolos encadenada por alcance
src/builtins.c    tabla de builtins: parametros y plantilla de G-code
src/evaluador.c   evaluacion de expresiones
src/semantico.c   primer recorrido: nombres, tipos y forma de las llamadas
src/posicion.c    segundo recorrido: posicion conocida, limites y feedrate
src/recorrido.c   lo que comparten el segundo recorrido y la generacion
src/codegen.c     tercer recorrido: generacion de G-code
src/errores.c     reporte de errores y advertencias
src/main.c        argumentos, orden de las fases y codigo de salida
tests/            programas de prueba y tests/esperado/ con el G-code esperado
ejemplos/         pieza para ver en un simulador
docs/             referencia y gramatica del lenguaje
vscode-mekoralang/  extension de VS Code
```
