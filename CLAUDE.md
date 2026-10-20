# MekoraLang

Compilador de MekoraLang, un lenguaje de alto nivel para escribir rutinas de mantenimiento y piezas simples para una impresora 3D Ender 3. El compilador se llama `gcodec`, está escrito en C con flex y bison, y traduce a G-code (firmware Marlin). No es un slicer: no lee modelos 3D ni calcula soportes o relleno.

Es el TP final grupal de la materia Compiladores (Ingeniería en Informática, UGD). Entrega y defensa el 14 de noviembre de 2026, con informe en formato LNCS (excluyente) y defensa de 15 minutos como máximo. El profesor es Ruidias y el TP se sigue en tutorías con él.

La definición completa del lenguaje está en `docs/especificacion-lenguaje.md` (gramática, tipos, tabla de builtins, los catorce chequeos semánticos, casos de prueba). `docs/referencia-lenguaje.md` es la versión corta para mostrar. Leé la especificación antes de tocar el análisis semántico o la generación.

## Cómo trabajar con nosotros

Somos estudiantes y empezamos desde cero en compiladores. Además del código necesitamos entender qué se hizo y por qué, porque lo vamos a defender oralmente. Entonces:

- Antes de un cambio grande, presentá un plan y esperá el visto bueno.
- Cuando la especificación no cubra un caso o haya más de una forma razonable de resolverlo, planteá las opciones con tu recomendación y preguntá. No decidas en silencio.
- Al terminar cada parte, explicá en prosa simple qué hiciste, por qué, y qué sección de la bibliografía lo respalda.
- Los documentos que vamos a mostrar a otras personas (informe, explicaciones, README) van en prosa simple y directa: sin tablas innecesarias, sin avisos en negrita, sin secciones de resumen. No queremos que se note de lejos que lo escribió una IA.

## Bibliografía y metodología

La bibliografía de la cátedra es Aho, Lam, Sethi y Ullman, "Compiladores: principios, técnicas y herramientas", 2ª edición (el "libro del dragón"), capítulos 1 a 8. Seguimos su estructura de fases y su terminología, y el informe la va a citar. Las secciones que corresponden a cada parte del proyecto:

- Estructura general y fases: 1.2. Alcance estático y estructura de bloques: 1.6.3.
- Análisis léxico con Lex/flex: 3.5.
- Análisis sintáctico LALR, precedencia, else colgante, recuperación de errores: 4.5 a 4.8 (sobre todo 4.8.1, 4.8.2 y 4.8.3).
- Árboles sintácticos y sintaxis abstracta: 2.5.1, 2.8.2, 5.3.1.
- Tabla de símbolos por alcance, encadenada, con la regla del bloque anidado más cercano: 2.7.
- Comprobación estática: 2.8.3. Definiciones dirigidas por la sintaxis y atributos sintetizados y heredados: 5.1 y 5.2. Tipos y declaraciones: 6.3. Comprobación de tipos por síntesis: 6.5.1 (sin conversiones implícitas, ver 6.5.2).
- Generación de código: capítulo 8, adaptado a que nuestro destino es G-code y no código de máquina.

El chequeo de posición conocida (sección 8.1 de la especificación) es un análisis de flujo de datos hacia adelante con unión conservadora en los puntos de confluencia. El libro lo trata en el capítulo 9, que no está en el material de la cátedra; en el informe lo presentamos como extensión, apoyándonos en las ideas de comprobación estática del 2.8.3.

## Herramientas y entorno

C11, flex 2.6, bison 3.6 o posterior (usamos 3.8.2), gcc y make. Desarrollamos en Windows: en una PC con Cygwin64 (proyecto en `C:\Users\User\Documents\MekoraLang`) y en otra con MSYS2 (`E:\MekoraLang\MekoraLang`). El código tiene que compilar igual en Cygwin, MSYS2 y Linux.

En Windows, Claude Code ejecuta los comandos en Git Bash, que no tiene make, flex, bison ni gcc. Para compilar hay que pasar por la shell de Cygwin. Por ejemplo, en la PC con Cygwin:

```
/c/cygwin64/bin/bash.exe -lc 'cd /cygdrive/c/Users/User/Documents/MekoraLang && make'
```

Si Cygwin está instalado en otra carpeta, o la PC usa MSYS2, ajustá la ruta (en MSYS2 el bash suele estar en `C:\msys64\usr\bin\bash.exe` y el disco C se ve como `/c/...`). Si no encontrás la shell, preguntá en lugar de improvisar otro toolchain.

Comandos del proyecto:

- `make` construye `gcodec`. Todo lo generado va a `build/`, que no se versiona.
- `make test` compila `tests/minimo.gc` y muestra el G-code.
- `make tokens PROG=archivo.gc` vuelca los tokens; `make ast PROG=archivo.gc` imprime el árbol sintáctico.
- `make clean` borra todo lo generado.
- `./gcodec entrada.gc [-o salida.gcode] [--tokens | --ast]`. Devuelve 0 si compiló y 1 si hubo errores.

El repositorio tiene `.gitattributes` con finales de línea LF. No conviertas archivos a CRLF.

## Estructura y convenciones del código

- `src/lexer.l`: analizador léxico. Lleva línea y columna y las pasa a bison por `yylloc`.
- `src/parser.y`: gramática completa, construcción del árbol y mensajes de error de sintaxis (`parse.error custom` con LAC, recuperación con `error PUNTO_COMA`).
- `src/ast.h` y `src/ast.c`: árbol sintáctico. Un único tipo `Nodo` con un campo `tipo` y una unión por clase de nodo; las listas se encadenan con `sig`; cada nodo guarda `linea` y `columna`. Hay constructores `ast_*`, `ast_imprimir` y `ast_liberar`.
- `src/errores.c`: reporte centralizado con formato gcc, `archivo:linea:columna: error <fase>: mensaje`, y contador global `errores`. Ninguna fase imprime errores por su cuenta.
- `src/codegen.c`: hoy solo prólogo y epílogo. `src/main.c`: argumentos, orquestación de fases y código de salida. `src/tokens.c`: nombres de tokens para `--tokens`.
- `tests/`: programas válidos y archivos `err_*.gc` con un error a propósito cada uno.

Convenciones:

- Identificadores, comentarios y mensajes en español. Los comentarios del código y los mensajes de error van sin tildes ni eñes (por compatibilidad de consola), como el código existente.
- Ante un error se reporta y se sigue, para mostrar todos los errores en una sola corrida. No abortar en el primero.
- El código propio compila sin advertencias con `-Wall`. El código que generan flex y bison se compila aparte con `GENFLAGS` (sin `-Wall`, con `-std=gnu11` porque flex usa `fileno()`).
- `bison -Wall` no puede reportar conflictos.
- No linkear con `-lfl`: el lexer usa `%option noyywrap`.
- El Makefile declara que `lex.yy.c` y `tokens.o` dependen de `parser.tab.h` y genera dependencias de headers con `-MMD -MP`. Si agregás un `.c`, sumalo a `OBJS`.
- Los builtins se implementan como una tabla de datos (nombre, parámetros admitidos con tipo y obligatoriedad, plantilla de G-code), no como un `switch` por builtin. Agregar un builtin tiene que costar una fila.
- No liberar memoria a medias: todo lo que se agregue al árbol o a la tabla de símbolos se libera al final. Verificar con AddressSanitizer cuando sea posible (en Linux; en Cygwin no está disponible).

## Estado

Paso 1, rebanada vertical: hecho. Toolchain completo, prólogo y epílogo fijos, códigos de salida.

Paso 2, análisis léxico: hecho. Palabras reservadas, ejes, operadores, números, identificadores, cadenas, línea y columna, errores léxicos.

Paso 3, análisis sintáctico y árbol: hecho (commit 3e81e8d). Gramática completa sin conflictos, árbol con posiciones, modo `--ast`, errores de sintaxis en español con recuperación.

Paso 4, análisis semántico: siguiente. Tabla de símbolos por alcance y los chequeos de la sección 8 de la especificación.

Paso 5, generación de G-code: desenrollado de `repeat`, expansión de procedimientos en línea, evaluación de expresiones, tabla de builtins con plantillas.

Paso 6, pruebas automatizadas: scripts que corren cada archivo de `tests/` y comparan código de salida, mensajes y G-code esperado, invocables desde el Makefile.

Calendario propuesto: paso 4 hasta el 25/10, paso 5 hasta el 1/11, paso 6 e informe hasta el 8/11, margen y ensayo hasta el 14/11.

Al cerrar cada paso, actualizá esta sección y la sección Estado del README.

## Decisiones tomadas en el paso 3, pendientes de confirmar con el grupo

- `AutoHome(X, Y)`: se agregó `arg -> eje` (un eje sin valor). El análisis semántico la tiene que restringir a `AutoHome`.
- Precedencia de `not`: entre `and` y las comparaciones, como en la EBNF. `not a > 3` es `not (a > 3)`.
- Exactamente un `main`: lo garantiza la gramática, así que es error sintáctico y no semántico.

Siguen abiertas las cuatro preguntas para Ruidias de la sección 13 de la especificación.
