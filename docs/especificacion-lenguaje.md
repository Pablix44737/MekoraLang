# Especificación del lenguaje (v6) — DSL de alto nivel → G-code

Decisiones cerradas: palabras reservadas en inglés y minúscula; estructuras de control limitadas a `if/else` + `repeat N`; vínculo con la máquina por parámetro explícito (sin `LoadMachine`); sin funciones con valor de retorno; modos de posicionamiento y extrusión fijados por el compilador; sin builtins de consulta al host; **tabla de builtins derivada de las operaciones del dominio, no de los comandos G-code**.

---

## 1. Criterio de diseño de la tabla de builtins

**Un builtin por operación distinta del dominio.** No un builtin por comando G-code.

Esta distinción es la que justifica la tabla entera y conviene enunciarla así en el informe. G-code mete el eje E dentro de `G1` como si fuera un cuarto eje cartesiano, pero desplazar el carro y empujar filamento son operaciones físicas distintas, ejecutadas por motores distintos, con consecuencias distintas ante un error. Que compartan opcode en el lenguaje destino es un detalle de implementación del destino, y taparlo es precisamente la función del DSL.

Dos consecuencias concretas del criterio:

- **`Move`, `Draw` y `Extrude` son tres builtins** aunque los tres emitan `G1`, porque son tres operaciones del dominio: desplazarse sin depositar material, depositar material a lo largo de una trayectoria, y empujar filamento sin desplazarse.
- **`Rapid` (`G0`) no existe**, aunque sea un comando G-code distinto, porque "desplazarse rápido" y "desplazarse" no son dos operaciones distintas para el operario — y en Marlin tampoco para la máquina: `G0` es un sinónimo de `G1` y ejecuta el mismo movimiento lineal coordinado. (Marlin 2.x tiene una opción de compilación, desactivada por defecto, que le daría a `G0` un feedrate propio; conviene verificarlo contra la versión de firmware de la máquina antes de afirmarlo en el informe.)

El criterio simétrico también vale: **ningún builtin es azúcar sintáctico de otro.** Cada operación del dominio tiene exactamente una forma de expresarse. Por eso `Move` **no** acepta `E:` — si lo aceptara, habría dos maneras de extruir y la separación entre `Move` y `Extrude` sería solo nominal.

---

## 2. Aclaración de niveles (para el informe y la defensa)

| Nivel | Formalismo | Jerarquía de Chomsky | Herramienta |
|---|---|---|---|
| Léxico (tokens) | Expresiones regulares | Tipo 3 — gramática regular | Flex |
| Sintáctico (estructura) | EBNF / BNF | Tipo 2 — libre de contexto | Bison (LALR(1)) |

Hablar de "la gramática regular del lenguaje" es incorrecto: los **tokens** son regulares, la **gramática del lenguaje** es libre de contexto. Los bloques anidados (`{ }` dentro de `{ }`) son la prueba directa de que el nivel sintáctico no puede ser regular.

---

## 3. Especificación léxica

### 3.1 Definiciones auxiliares (Flex)

```
DIGITO      [0-9]
LETRA       [a-zA-Z_]
ID          {LETRA}({LETRA}|{DIGITO})*
NUMERO      {DIGITO}+("."{DIGITO}+)?
CADENA      \"[^\"\n]*\"
COMENTARIO  "//"[^\n]*
ESPACIO     [ \t\r\n]+
```

Los comentarios y los espacios se descartan (no producen token).

### 3.2 Palabras reservadas

Todas en **minúscula**, case-sensitive:

```
machine   procedure   main   const   var
if        else        repeat  as
number    bool        true    false
and       or          not
```

### 3.3 Tokens de eje/parámetro (reservados, en MAYÚSCULA)

```
X   Y   Z   E   F   S
```

Al ser mayúsculas y las palabras clave minúsculas, no hay colisión: `var x : number = 5;` es válido, `X` como identificador no lo es.

### 3.4 Operadores y símbolos

```
+  -  *  /  %
==  !=  <  >  <=  >=
=  .  ,  ;  :
(  )  {  }
```

### 3.5 Nota de implementación en Flex

Las palabras reservadas se reconocen con reglas explícitas **ubicadas antes** de la regla `{ID}`. Flex aplica el criterio de coincidencia más larga y, ante empate, la regla que aparece primero en el archivo. Alternativa equivalente y más mantenible: una única regla `{ID}` que consulta una tabla de palabras reservadas y devuelve el token correspondiente.

---

## 4. Gramática (EBNF)

### 4.1 Estructura del programa

```ebnf
program        ::= { declaracion } main_block
declaracion    ::= machine_decl | const_decl | procedure_decl

machine_decl   ::= "machine" ID "{" campo { campo } "}"
campo          ::= ID ":" expr ";"

const_decl     ::= "const" ID ":" tipo "=" expr ";"

procedure_decl ::= "procedure" ID "(" [ lista_param ] ")" bloque
lista_param    ::= param { "," param }
param          ::= ID ":" tipo

tipo           ::= "number" | "bool" | "machine"

main_block     ::= "main" bloque
bloque         ::= "{" { sentencia } "}"
```

### 4.2 Sentencias

```ebnf
sentencia      ::= var_decl
                 | asignacion
                 | if_stmt
                 | repeat_stmt
                 | llamada
                 | bloque

var_decl       ::= "var" ID ":" tipo "=" expr ";"
asignacion     ::= ID "=" expr ";"

if_stmt        ::= "if" "(" expr ")" bloque [ "else" ( bloque | if_stmt ) ]

repeat_stmt    ::= "repeat" expr [ "as" ID ] bloque

llamada        ::= ID "(" [ lista_arg ] ")" ";"
lista_arg      ::= arg { "," arg }
arg            ::= expr | eje ":" expr | eje | CADENA
eje            ::= "X" | "Y" | "Z" | "E" | "F" | "S"
```

La alternativa `arg ::= eje` (un eje sin valor) se agregó en el paso 3 para que `AutoHome(X, Y)` sea escribible. El análisis semántico la restringe a `AutoHome`.

### 4.3 Expresiones (precedencia de menor a mayor)

```ebnf
expr           ::= or_expr
or_expr        ::= and_expr { "or" and_expr }
and_expr       ::= not_expr { "and" not_expr }
not_expr       ::= [ "not" ] rel_expr
rel_expr       ::= add_expr [ relop add_expr ]
relop          ::= "==" | "!=" | "<" | ">" | "<=" | ">="
add_expr       ::= mul_expr { ( "+" | "-" ) mul_expr }
mul_expr       ::= unario { ( "*" | "/" | "%" ) unario }
unario         ::= [ "-" ] primario
primario       ::= NUMERO
                 | "true" | "false"
                 | ID [ "." ID ]
                 | "(" expr ")"
```

En Bison, la precedencia puede declararse con `%left` / `%right` en lugar de estratificar las producciones. Estratificarlas (como arriba) es más verboso pero deja la precedencia documentada en la propia gramática, lo cual conviene para el informe.

### 4.4 Dos propiedades de la gramática a señalar en la defensa

**Llaves obligatorias ⇒ no hay dangling-else.** Al exigir `bloque` (con llaves) en las ramas del `if`, el conflicto clásico de ambigüedad `if-if-else` desaparece por construcción. Bison no reporta conflictos shift/reduce por este motivo.

**Los argumentos nombrados no generan conflicto.** Ante `Move(X: 10)`, al leer el token `X` el parser tiene lookahead `:`. Como `:` no pertenece al conjunto FOLLOW de una expresión, LALR(1) decide sin ambigüedad entre `arg → eje ":" expr` y `arg → expr`. Reservar los ejes como tokens propios hace que esto sea trivialmente correcto.

---

## 5. Sistema de tipos

Tres tipos: `number` (real), `bool`, `machine`. Cómo se escribe un número en el G-code está en la sección 10.1.

- No hay tipo `string`: las cadenas existen únicamente como literal argumento de builtins (`Message`, `Raw`).
- No hay conversión implícita entre `number` y `bool`.
- El tipo `machine` solo aparece como tipo de parámetro; no se puede declarar una variable ni una constante de tipo `machine`, ni asignar una máquina. Un nombre de máquina solo puede aparecer como argumento de un parámetro `machine` o a la izquierda de un punto (`Ender3.limite_x`).
- Los campos de un bloque `machine` son siempre `number`.

El tipo de cada expresión se calcula a partir de los tipos de sus partes:

- `+ - * / %` y el menos unario se aplican a `number` y dan `number`. `%` es el resto real: `7.5 % 2` vale 1.5.
- `< > <= >=` comparan dos `number` y dan `bool`.
- `==` y `!=` comparan dos `number` o dos `bool` y dan `bool`.
- `and`, `or` y `not` se aplican a `bool` y dan `bool`. `and` y `or` evalúan siempre los dos lados: no hay cortocircuito, así que en `x != 0 and 10 / x > 1` con `x` en 0 hay una división por cero.

Una división o un resto por cero en una expresión que se evalúa al compilar es un error.

### 5.1 La máquina

Un programa se compila para una impresora concreta, así que declara exactamente una máquina, y esa declaración es la primera del programa. Si se quiere el mismo programa para dos impresoras, son dos programas.

Toda máquina tiene que declarar cuatro campos, que son los únicos nombres de campo con significado para el compilador:

- `limite_x`, `limite_y`, `limite_z`: el recorrido de cada eje en milímetros. Toda coordenada de `Move` y `Draw` tiene que estar entre 0 y el límite.
- `feedrate_max`: la velocidad máxima en mm/min. Todo `F:` tiene que ser mayor que 0 y no superarla.

Los cuatro tienen que ser mayores que 0. Además de esos, el programador puede agregar los campos que quiera (por ejemplo, las temperaturas de trabajo) y leerlos con el punto; el compilador no les da ningún significado.

Como hay una sola máquina, un parámetro de tipo `machine` siempre es esa máquina, y el acceso `m.campo` se verifica al analizar el procedimiento, sin esperar a la llamada.

---

## 6. Modos fijos: prólogo y epílogo automáticos

**El programador no elige ni puede cambiar el modo de posicionamiento ni el de extrusión.** El compilador los fija en un prólogo que emite siempre:

```gcode
; Generado automaticamente - no editar
G21          ; unidades en milímetros
G90          ; posicionamiento absoluto (X, Y, Z)
M83          ; extrusión relativa (E)
```

### Por qué X/Y/Z en absoluto

- La **validación de límites es exacta**: el argumento de `Move(X: 180)` es directamente la coordenada destino, comparable contra `limite_x` sin análisis adicional.
- No hay que rastrear la posición acumulada a través de las ramas de un `if`, que es donde el backend se vuelve genuinamente difícil.
- No se acumulan errores de redondeo a lo largo de un `repeat` desenrollado.
- La variación dentro de un bucle se resuelve con el índice de `repeat ... as i`.

### Por qué E en relativo

El eje E es **acumulativo y sin límite físico**. En modo absoluto, para extruir 10 mm el compilador debería conocer el valor actual de E y emitir `G1 E<anterior+10>`, llevando un contador a lo largo de todo el programa incluyendo bucles desplegados y ambas ramas de cada condicional. Con `M83`, `Extrude(E: 10)` significa siempre "extruir 10 mm más", sin estado que rastrear.

**Consecuencia de diseño:** `SetAbsolute()` y `SetRelative()` no existen en el lenguaje. Un modo que no se puede cambiar es un modo que el compilador no necesita rastrear.

### Epílogo automático (seguridad)

Al final de todo programa, el compilador emite incondicionalmente:

```gcode
M104 S0      ; apagar extrusor
M140 S0      ; apagar cama
M107         ; apagar ventilador
M84          ; deshabilitar motores
```

Se emite aunque el programador lo haya escrito explícitamente (el comando repetido es inocuo) y, sobre todo, **aunque se haya olvidado**. Es una garantía que el compilador da y el G-code escrito a mano no.

---

## 7. Tabla de builtins

### Movimiento y material

| Builtin | Operación del dominio | G-code |
|---|---|---|
| `Move(X:, Y:, Z:, F:)` | Desplazarse sin depositar material | `G1 X.. Y.. Z.. F..` |
| `Draw(X:, Y:, Z:, E:, F:)` | Depositar material a lo largo de una trayectoria | `G1 X.. Y.. Z.. E.. F..` |
| `Extrude(E:, F:)` | Empujar filamento sin desplazarse | `G1 E.. F..` |
| `AutoHome(X:, Y:, Z:)` — ejes opcionales | Referenciar los ejes contra los finales de carrera | `G28` / `G28 X Y` |

En los tres primeros, todos los parámetros de eje son opcionales pero debe haber al menos uno; `F:` es opcional (si se omite, la máquina usa el último feedrate). `Draw` exige `E:` y al menos un eje de posición — sin ambos no es una operación de dibujo.

### Temperatura

| Builtin | Operación | G-code |
|---|---|---|
| `SetExtruderTemp(n)` | Fijar temperatura de extrusor sin esperar | `M104 S<n>` |
| `WaitExtruderTemp(n)` | Fijar y esperar a alcanzarla | `M109 S<n>` |
| `SetBedTemp(n)` | Fijar temperatura de cama sin esperar | `M140 S<n>` |
| `WaitBedTemp(n)` | Fijar y esperar a alcanzarla | `M190 S<n>` |
| `TurnOffHeaters()` | Dejar la máquina fría | `M104 S0` + `M140 S0` |

`TurnOffHeaters()` es la única entrada que mapea a dos comandos. Bajo el criterio de dominio eso no es un problema: "dejar la máquina fría" es una operación, se emitan uno o cinco comandos.

### Control y mantenimiento

| Builtin | Operación | G-code |
|---|---|---|
| `Wait(segundos)` | Pausar la ejecución | `G4 S<n>` |
| `Fan(S:)` — `S: 0` apaga | Fijar velocidad del ventilador | `M106 S<n>` |
| `DisableMotors()` | Liberar los motores para manipulación manual | `M84` |
| `Message("texto")` | Mostrar texto al operario en el LCD | `M117 texto` |
| `Raw("...")` | Válvula de escape | emite la línea literal |

`Fan(S: 0)` emite `M106 S0`, que equivale a `M107`; no hace falta un caso especial en el generador.

El texto de `Message` no puede tener `;`: en G-code el punto y coma empieza un comentario, y Marlin mostraría el texto cortado. `Raw` no tiene esa restricción, porque quien la usa está escribiendo G-code.

`DisableMotors()` es una operación de mantenimiento genuina, no azúcar del epílogo: con los steppers energizados los ejes quedan retenidos y no se pueden mover a mano. Liberarlos es requisito para manipular la máquina físicamente. Ver la sección 8.1, porque tiene una consecuencia semántica importante.

### Builtins descartados y por qué

- **`Rapid()` (G0)** — En Marlin es sinónimo de `G1`; "desplazarse rápido" no es una operación distinta del dominio. Si se quisiera el guiño de legibilidad de los slicers, el compilador puede derivarlo solo (`Move` → `G0`, `Draw` → `G1`) con una condición en el generador y cero sentencias nuevas en el lenguaje. Es puramente cosmético.
- **`SetAbsolute()` / `SetRelative()`** — Los modos son fijos (sección 6).
- **`ReportPosition()` (M114), `ReportTemps()` (M105), `ReadEndstops()` (M119)** — Son comandos de *consulta*: responden por el puerto serie al host que los envió. Solo tienen efecto observable desde OctoPrint, Pronterface o similar conectado por USB; si el `.gcode` se imprime desde la tarjeta SD, la respuesta no la lee nadie. Quien los necesite los emite con `Raw("M119")`.

### Nota de implementación

Los builtins deben implementarse como una **tabla de datos** (nombre, parámetros admitidos con su tipo y obligatoriedad, plantilla de G-code), no como un `case` por builtin en el generador. Con esa arquitectura el chequeo de aridad y tipos recorre la tabla, y agregar un builtin cuesta una fila.

En la implementación (`src/builtins.c`) cada parámetro dice si es posicional (`Wait(2)`), nombrado con un eje (`Move(X: 10)`), un eje sin valor (`AutoHome(X)`) o una cadena (`Message("...")`), su tipo, si es obligatorio y con qué letra sale en el G-code. Cada fila dice además si el builtin necesita al menos un eje de posición, qué efecto tiene sobre la posición conocida (ninguno, mueve, referencia o libera) y su plantilla. De esos datos salen todos los chequeos de forma de las llamadas: aridad, tipos, `Draw` sin `E:`, `Move` sin ningún eje, ejes que el builtin no admite y ejes repetidos.

---

## 8. Análisis semántico

### 8.1 Validez de la posición (el chequeo distintivo del compilador)

El compilador mantiene, por cada eje, un booleano **"posición conocida"**. Tres booleanos en total.

- **Al inicio del programa los tres están en falso.** Una impresora recién encendida no sabe dónde está.
- **`AutoHome()`** los pone en verdadero. `AutoHome(X:, Y:)` pone en verdadero solo los ejes nombrados.
- **`DisableMotors()`** los pone todos en falso: liberados los motores, el operario puede mover la máquina a mano y la posición real deja de ser la que el compilador cree.
- **`Move` o `Draw` sobre un eje cuya posición no es conocida es un error de compilación.**

En un `if`, la posición de un eje queda en verdadero después del condicional solo si lo está en **todas** las ramas (es el join conservador de un análisis de flujo de datos; en el informe conviene nombrarlo así).

El análisis no mira el valor de la condición: recorre las dos ramas aunque la condición sea una constante. Es una decisión, no una limitación. El lenguaje no tiene entradas, así que en la generación toda condición se puede evaluar, y como Marlin no tiene condicionales en G-code el generador emite solo la rama que se toma (sección 10.1). Pero el análisis semántico no depende de esos valores: la garantía que da vale para cualquier valor de la condición, y seguiría valiendo si el lenguaje algún día tuviera entradas.

Las llamadas a procedimientos se analizan como si el cuerpo estuviera escrito en el lugar de la llamada, que es lo que hace el generador. El análisis empieza en `main` y, en cada llamada, entra al procedimiento con el estado de los tres ejes en ese punto y sale con el que deja el cuerpo. Por eso en el ejemplo de la sección 9 `Preparar` puede hacer el `AutoHome()` y `BarridoCama` mover después. Si el error está dentro de un procedimiento, el mensaje se marca en la sentencia de adentro y lo siguen notas que dicen desde qué llamadas se llegó.

El cuerpo de un `repeat` se recorre tantas veces como se repite, igual que el desenrollado. Así se ve el caso de un cuerpo que hace `DisableMotors()` y en la vuelta siguiente mueve, y en cada vuelta el índice tiene su valor para los límites. Un `repeat 0` no recorre el cuerpo, porque nunca se ejecuta.

Los procedimientos que `main` nunca llama no pasan por este análisis: no generan G-code y nunca se ejecutan. El resto de los chequeos sí se les aplica.

Este chequeo vale doble:

1. Detecta el error real de mover antes de referenciar. Marlin, en su configuración por defecto, acepta movimientos sin homing previo, así que la máquina no te protege.
2. Detecta el error que tu propio `DisableMotors()` habilita: mover en absoluto después de que un humano corrió los ejes a mano.

Ninguno de los dos es detectable en G-code crudo bajo ninguna circunstancia. Es el argumento más fuerte del proyecto para la sección de *safety*.

### 8.2 Resto de los chequeos

1. **Identificador no declarado** — variable, constante, máquina o procedimiento inexistente.
2. **Redeclaración** en el mismo ámbito.
3. **Compatibilidad de tipos** en asignaciones, operadores y argumentos.
4. **Condición de `if` debe ser `bool`.**
5. **Aridad y tipos en llamadas** a procedimientos y builtins.
6. **Campo inexistente** en acceso `m.campo`.
7. **`repeat` con expresión constante entera no negativa** — se evalúa en compilación porque el desenrollado lo exige.
8. **Asignación a constante** — error.
9. **Asignación al índice de `repeat`** — error (es de solo lectura).
10. **Existencia de exactamente un `main`.** (Desde el paso 3 lo garantiza la gramática: un programa sin `main` o con dos es un error sintáctico.)
11. **`Draw` sin `E:` o sin eje de posición** — error.
12. **Al menos un parámetro de eje** en `Move`, `Draw`, `Extrude`.
13. **Posición conocida** (sección 8.1).
14. **Límites físicos** y feedrate máximo (abajo).

Chequeos que se agregaron en el paso 4, al resolver los casos que la lista anterior no cubría:

15. **Exactamente una máquina**, declarada primero y con los cuatro campos obligatorios mayores que 0 (sección 5.1).
16. **Declarar antes de usar** y **recursión prohibida** (sección 8.4).
17. **Forma de los argumentos de un builtin**: ejes que el builtin no admite (`Move(E: 5)` es error, por el criterio de la sección 1), ejes repetidos, eje sin valor fuera de `AutoHome`, eje con valor en `AutoHome`, cadena donde va un número y número donde va una cadena.
18. **Procedimientos con argumentos posicionales**: no reciben ejes ni cadenas.
19. **Asignación a un parámetro** — error (es de solo lectura, como el índice).
20. **Nombres de builtins reservados**: no se puede declarar nada llamado `Move`, `Wait`, etc.
21. **`;` en el texto de `Message`** — error (sección 7).
22. **División o resto por cero** en una expresión que se evalúa al compilar.

### 8.3 Validación de límites

Con el posicionamiento fijado en absoluto, el argumento de cada `Move` / `Draw` es directamente la coordenada destino:

- Si el argumento es una **expresión constante**: se evalúa y se emite **error de compilación** si queda fuera de `[0, limite_eje]`.
- Si depende de una variable: **advertencia**, y se emite un comentario en el G-code generado. Al generar, el valor real ya se conoce: si queda fuera de rango es un error (sección 10.2).

Lo mismo vale para todo `F:` (en `Move`, `Draw` y `Extrude`), que tiene que ser mayor que 0 y no superar `feedrate_max`.

Una expresión es constante si solo usa literales, constantes, campos de la máquina (`Ender3.limite_x`, o `m.limite_x` con `m` parámetro), índices de `repeat` y parámetros cuyo argumento en la llamada fue constante. La definición es recursiva: en `Cuadrado(80, 80, 40)`, `origen_x + lado` es constante porque los dos argumentos lo son. Las variables (`var`) nunca son constantes, aunque su valor se pueda deducir, porque se pueden reasignar. Esto exige que los parámetros sean de solo lectura.

La misma definición vale para la cantidad de un `repeat`, que además tiene que ser un entero no negativo. Si usa una variable, el error se da al analizar el procedimiento. Si usa un parámetro, el valor se conoce en cada llamada, y el error, si lo hay, sale con la nota de la llamada.

El índice de un `repeat` empieza en 0: `repeat 4 as fila` le da a `fila` los valores 0, 1, 2 y 3. Si un movimiento se sale de los límites en una vuelta, el mensaje dice el valor del índice en esa vuelta.

Como el `repeat` se desenrolla por completo, la enorme mayoría de los movimientos cae en el primer caso. En el ejemplo de la sección 9, los dos `F: vel` de `TestEjes` dan advertencia, porque `vel` es una variable.

### 8.4 Alcance y orden de las declaraciones

Los nombres se resuelven con una tabla de símbolos por alcance y la regla del bloque anidado más cercano. Los alcances son estos, de afuera hacia adentro: uno predefinido con los builtins, el global (la máquina, las constantes y los procedimientos), uno por procedimiento y uno por `main`, y uno por cada bloque interno, rama de `if` o cuerpo de `repeat`. Los campos de la máquina están en un alcance aparte, al que solo se llega con el punto.

Reglas:

- **Todo nombre se declara antes de usarse**, incluidos los procedimientos: un procedimiento solo puede llamar a los que están declarados más arriba. Como `main` va al final, puede llamar a todos. Si se usa un nombre declarado más abajo, el mensaje lo dice.
- **No hay recursión.** Cada llamada se expande en línea, así que una llamada recursiva no terminaría. Con la regla anterior la recursión indirecta no se puede escribir: para que A llame a B y B a A, uno de los dos tendría que llamar a un procedimiento declarado más abajo. Queda la directa, que es un error con mensaje propio.
- **Redeclarar en el mismo alcance es error. Ocultar es válido:** un bloque interno puede declarar un nombre que ya existe afuera (una variable, un parámetro o una constante), y adentro del bloque vale el de adentro.
- Los parámetros comparten alcance con el cuerpo del procedimiento: `procedure P(a: number) { var a ... }` es una redeclaración. Lo mismo el índice de un `repeat` con el cuerpo del `repeat`. Los bloques internos sí pueden ocultarlos.
- Hay un solo espacio de nombres: una variable llamada `Preparar` oculta al procedimiento `Preparar`, y llamarla da "no es un procedimiento".
- Los nombres de los builtins están reservados.
- En `var a : number = a + 1;` la `a` de la derecha es la de afuera: la variable existe recién después de su declaración.
- El valor de una constante o de un campo tiene que ser una expresión constante hecha con literales y constantes declaradas antes.

### 8.5 Organización del análisis

El análisis se hace en dos recorridos del árbol.

El primero recorre las declaraciones en el orden del fuente y después `main`. Arma la tabla de símbolos, resuelve los nombres, calcula los tipos y verifica la forma de cada llamada, las asignaciones y los `repeat` con variables. Deja el árbol decorado: cada uso de un nombre queda apuntando a su entrada en la tabla.

El segundo empieza en `main` y recorre el programa como se va a ejecutar, con la expansión en línea y el desenrollado descritos en la sección 8.1. Hace los chequeos que dependen del camino: posición conocida, límites, feedrate, el valor de cada `repeat` y las divisiones por cero que dependen de un parámetro.

El segundo recorrido corre solo si el primero terminó sin errores. Con nombres sin resolver o tipos equivocados no tiene sentido evaluar, y se evitan errores en cascada. Por eso un programa con un error de tipos no muestra todavía sus errores de posición.

Cada error del segundo recorrido se informa una vez por sentencia y por cadena de llamadas: si un cuerpo de `repeat` falla en todas las vueltas, el error sale una sola vez, en la primera.

---

## 9. Programa de ejemplo

```
// ---------- Definición de máquina ----------
machine Ender3 {
    temp_extrusor: 210;
    temp_cama: 60;
    limite_x: 220;
    limite_y: 220;
    limite_z: 250;
    feedrate_max: 3000;
}

const MARGEN : number = 10;
// mm de filamento por mm de trayectoria (boquilla 0.4, capa 0.2, filamento 1.75)
const FLUJO  : number = 0.033;

// ---------- Mantenimiento ----------
procedure Preparar(m: machine) {
    SetBedTemp(m.temp_cama);
    SetExtruderTemp(m.temp_extrusor);
    WaitBedTemp(m.temp_cama);
    WaitExtruderTemp(m.temp_extrusor);
    AutoHome();
}

procedure PurgarBoquilla() {
    repeat 5 {
        Extrude(E: 10, F: 300);
        Wait(2);
    }
}

procedure LiberarParaMantenimiento() {
    TurnOffHeaters();
    Message("Motores liberados - mover a mano");
    DisableMotors();
}

// ---------- Diagnóstico ----------
procedure TestEjes(m: machine) {
    var vel : number = m.feedrate_max;

    if (vel > 3000) {
        vel = 3000;
        Message("Feedrate limitado");
    }

    Move(X: MARGEN,               Y: MARGEN,               F: vel);
    Move(X: m.limite_x - MARGEN,  Y: m.limite_y - MARGEN,  F: vel);
    Move(Z: m.limite_z / 2, F: 1500);
    AutoHome();
}

procedure BarridoCama(m: machine) {
    repeat 4 as fila {
        Move(X: MARGEN,               Y: MARGEN + fila * 50, F: m.feedrate_max);
        Move(X: m.limite_x - MARGEN,  Y: MARGEN + fila * 50, F: m.feedrate_max);
    }
}

// ---------- Pieza simple ----------
procedure Cuadrado(origen_x: number, origen_y: number, lado: number) {
    Move(X: origen_x, Y: origen_y, Z: 0.3, F: 3000);
    Draw(X: origen_x + lado, Y: origen_y,        E: lado * FLUJO, F: 1200);
    Draw(X: origen_x + lado, Y: origen_y + lado, E: lado * FLUJO, F: 1200);
    Draw(X: origen_x,        Y: origen_y + lado, E: lado * FLUJO, F: 1200);
    Draw(X: origen_x,        Y: origen_y,        E: lado * FLUJO, F: 1200);
}

// ---------- Programa principal ----------
main {
    Preparar(Ender3);
    TestEjes(Ender3);
    PurgarBoquilla();
    BarridoCama(Ender3);
    Cuadrado(80, 80, 40);
}
```

`main` no apaga nada: el epílogo automático se encarga.

Si a `LiberarParaMantenimiento()` le siguiera un `Move`, el compilador lo rechaza por posición desconocida hasta que aparezca un `AutoHome()`.

El programa compila con dos advertencias, una por cada `F: vel` de `TestEjes`: `vel` es una variable, así que el compilador no puede verificar al compilar que no supere `feedrate_max`. `LiberarParaMantenimiento` no se llama desde `main`, así que no pasa por el chequeo de posición (sección 8.1).

### Cambios acumulados respecto de la propuesta original

| Antes | Ahora | Motivo |
|---|---|---|
| `Machine`, `Procedure`, `Main` | `machine`, `procedure`, `main` | Distinción visual keyword / identificador |
| `LoadMachine X as Y;` | eliminado | El bloque `machine` ya declara la instancia |
| `Procedure P(Machine m)` | `procedure P(m: machine)` | Coherente con `var x : number` |
| `TurnOffHeaters(MiMaquina)` | `TurnOffHeaters()` | El builtin no usa la máquina |
| sin estructuras de control | `if/else`, `repeat [as i]` | Requisito del paradigma estructurado |
| sin variables locales | `var`, `const`, asignación | Idem |
| `SetAbsolute()` / `SetRelative()` | eliminados; modos fijos en el prólogo | Elimina el rastreo de estado en el backend |
| — | epílogo automático de apagado | Garantía de seguridad |
| — | `ReportPosition` / `ReportTemps` / `ReadEndstops` descartados | Solo observables con host conectado por serie |
| — | `Rapid` descartado | `G0` es sinónimo de `G1` en Marlin |
| `Move` con `E:` | `Move` sin `E:`; se agrega `Draw` | Un builtin por operación del dominio, ninguno azúcar de otro |
| — | chequeo de posición conocida | Detecta movimiento sin homing y tras `DisableMotors()` |
| cualquier cantidad de máquinas, campos libres | exactamente una máquina, primera, con cuatro campos obligatorios | Un programa es para una impresora; el compilador necesita sus límites y su feedrate |
| — | `F:` verificado contra `feedrate_max` | La misma garantía que los límites, para la velocidad |
| orden libre de declaraciones | declarar antes de usar, sin recursión | Una sola regla, y hace imposible la recursión indirecta |

Convención de nombres vigente: **palabras clave y builtins en inglés; identificadores (máquinas, campos, procedimientos, variables) los nombra el usuario libremente.** Por eso conviven `machine Ender3 { temp_extrusor: 210; }` y `SetExtruderTemp(...)` sin incoherencia.

---

## 10. Muestra de salida generada

Fuente:

```
machine Ender3 { temp_extrusor: 210; temp_cama: 60;
                 limite_x: 220; limite_y: 220; limite_z: 250;
                 feedrate_max: 3000; }

procedure PurgarBoquilla() {
    repeat 3 {
        Extrude(E: 10, F: 300);
        Wait(2);
    }
}

main {
    AutoHome();
    PurgarBoquilla();
}
```

G-code generado:

```gcode
; Generado automaticamente - no editar
G21
G90
M83
G28
G1 E10.000 F300.000
G4 S2
G1 E10.000 F300.000
G4 S2
G1 E10.000 F300.000
G4 S2
M104 S0
M140 S0
M107
M84
```

El contraste entre el bucle explícito del fuente y su desenrollado en la salida —más el prólogo y el epílogo que el programador nunca escribió— es el mejor material para la introducción del informe y para la demo de la defensa.

### 10.1 Cómo se genera

G-code no tiene variables, saltos ni condicionales, y el lenguaje no tiene entradas, así que todo valor se conoce al compilar. Generar es ejecutar el programa en el compilador y escribir lo que la máquina haría. Es un tercer recorrido del árbol, que empieza en `main` y corre solo si el análisis terminó sin errores:

- Un `repeat` se desenrolla: su cuerpo se emite una vez por vuelta.
- Un procedimiento se expande en el lugar de cada llamada.
- Una variable guarda su valor en su entrada de la tabla de símbolos. Alcanza con una sola, porque no hay recursión. Una `var` declarada dentro de un `repeat` se reinicia en cada vuelta.
- De un `if` se evalúa la condición y se emite solo la rama que se toma.
- Cada builtin sale de su fila en la tabla: la plantilla y, en el orden de la tabla, los parámetros presentes. `Move(Y: 2, X: 1)` da `G1 X1.000 Y2.000`.

No hay código intermedio: del árbol se pasa directo a G-code.

`X`, `Y`, `Z`, `E` y `F` salen con 3 decimales. `S` (temperatura, segundos, ventilador) sale sin decimales si es entero y con 3 si no lo es: `M140 S60`, `G4 S0.500`.

Donde el análisis dio una advertencia, antes de la línea se emite un comentario por cada argumento, por ejemplo `; advertencia (linea 39): F depende de una variable`. Si un procedimiento se llama con un argumento variable y otro constante, el comentario sale solo en la expansión que corresponde.

### 10.2 Errores de generación

El análisis no conoce el valor de las variables, pero la generación sí. Hay dos errores que aparecen recién acá, con la clase `error generacion`:

- Un valor que depende de una variable y queda fuera de los límites, o un `F` que no cumple con `feedrate_max`. El análisis dio la advertencia, y al generar se ve que el valor real está mal.
- Una división o un resto por cero que depende de una variable.

Se informan una vez por sentencia y por cadena de llamadas, como en el segundo recorrido. Si hay un error, no queda archivo de salida y el compilador devuelve 1: ese G-code no se le puede dar a la máquina.

Una rama que no se toma nunca se ejecuta, así que un valor variable fuera de rango en esa rama no es error de generación. Un valor constante fuera de rango sí es error en cualquier rama, porque el análisis no mira la condición (sección 8.1).

---

## 11. Escenarios de prueba (requisito de la cátedra)

Casos válidos: programa mínimo (la máquina con solo los campos obligatorios y un `main` vacío; desde el paso 4 la máquina es obligatoria y el caso "máquina con un único campo" dejó de ser válido), procedimiento sin parámetros, `if` sin `else`, `if/else if/else` encadenado, `repeat` anidado, `repeat` con índice, expresión aritmética con precedencia mixta, `Draw` completo, `DisableMotors()` seguido de `AutoHome()` y movimiento, uso de `Raw`. En el paso 4 se agregaron: el programa de la sección 9 completo (`ejemplo_completo.gc`), homing en todas las ramas de un `if` (`pos_if_ramas.gc`), `AutoHome(X, Y)` parcial, ocultamiento de nombres y un límite que depende de una variable, que compila con una advertencia (`adv_limite_variable.gc`).

Cada archivo de prueba dice en su segunda línea qué se espera: "compila sin errores" o el error, con la línea y un fragmento del mensaje.

Casos de error, uno por chequeo de la sección 8:

| Archivo | Error esperado |
|---|---|
| `err_no_declarado.gc` | uso de variable inexistente |
| `err_redeclaracion.gc` | dos `var` con el mismo nombre en el mismo ámbito |
| `err_tipo_cond.gc` | `if (5) { }` — condición no booleana |
| `err_tipo_asig.gc` | `var b : bool = 3;` |
| `err_aridad.gc` | `Wait(1, 2);` |
| `err_campo.gc` | `m.campo_inexistente` |
| `err_repeat_no_const.gc` | `repeat vel { }` con `vel` variable |
| `err_asig_const.gc` | asignación a una constante |
| `err_asig_indice.gc` | asignación al índice de un `repeat` |
| `err_sin_main.gc` | programa sin `main` (error sintáctico desde el paso 3) |
| `err_draw_sin_e.gc` | `Draw(X: 10, F: 1200);` sin `E:` |
| `err_move_vacio.gc` | `Move(F: 3000);` sin ningún eje |
| `err_sin_homing.gc` | `Move` como primera sentencia de `main` |
| `err_pos_invalida.gc` | `Move` después de `DisableMotors()` sin re-homing |
| `err_pos_rama.gc` | `DisableMotors()` en una sola rama de un `if`, `Move` después |
| `err_limite.gc` | `Move(X: 500)` con `limite_x: 220` |
| `err_lexico.gc` | carácter no reconocido (`@`) |
| `err_sintactico.gc` | `;` faltante |

Casos de error agregados en el paso 4, por las decisiones de las secciones 5.1, 8.3 y 8.4:

| Archivo | Error esperado |
|---|---|
| `err_sin_maquina.gc` | programa sin máquina |
| `err_dos_maquinas.gc` | dos máquinas |
| `err_maquina_no_primera.gc` | la máquina no es la primera declaración |
| `err_campo_faltante.gc` | falta `feedrate_max` |
| `err_campo_no_positivo.gc` | `limite_z: 0` |
| `err_recursion.gc` | un procedimiento que se llama a sí mismo |
| `err_uso_antes.gc` | llamada a un procedimiento declarado más abajo |
| `err_eje_no_admitido.gc` | `Move(X: 10, E: 5)` |
| `err_eje_repetido.gc` | `Move(X: 10, X: 20)` |
| `err_eje_solo.gc` | `Move(X)` |
| `err_autohome_valor.gc` | `AutoHome(X: 10)` |
| `err_cadena_arg.gc` | `Wait("dos")` |
| `err_tipo_op.gc` | `3 + true` |
| `err_tipo_arg.gc` | `SetBedTemp(true)` |
| `err_aridad_proc.gc` | procedimiento con argumentos de menos |
| `err_var_machine.gc` | `var m : machine = Ender3;` |
| `err_const_machine.gc` | `const M : machine = Ender3;` |
| `err_repeat_negativo.gc` | `repeat -2` |
| `err_repeat_no_entero.gc` | `repeat 2.5` |
| `err_repeat_param.gc` | `repeat n` con `n` parámetro que recibe una variable |
| `err_pos_proc.gc` | posición desconocida dentro de un procedimiento, con la nota de la llamada |
| `err_limite_indice.gc` | límite excedido en una vuelta de un `repeat`, con el valor del índice |
| `err_limite_param.gc` | límite excedido por el argumento de una llamada |
| `err_feedrate.gc` | `F: 5000` con `feedrate_max: 3000` |
| `err_asig_param.gc` | asignación a un parámetro |
| `err_nombre_builtin.gc` | `procedure Move()` |
| `err_message_punto_coma.gc` | `Message("Calentando; esperar")` |
| `err_div_cero.gc` | `10 / 0` en una constante |
| `err_no_procedimiento.gc` | llamada a una variable |

Casos agregados en el paso 5, por la generación (sección 10). De cada programa válido hay un G-code esperado en `tests/esperado/`. El de los casos `gen_*` se escribió a mano, sin mirar la salida del compilador; el de los demás se guardó después de revisarlo contra su fuente.

| Archivo | Qué prueba |
|---|---|
| `gen_muestra.gc` | la muestra de esta sección 10, línea por línea |
| `gen_builtins.gc` | todos los builtins, el orden de los parámetros y el formato de los números |
| `gen_variables.gc` | reasignación, `var` que se reinicia en cada vuelta, comentarios de advertencia |
| `gen_if.gc` | solo se emite la rama que se toma, incluso con `else if` |
| `gen_repeat.gc` | `repeat 0`, índice desde 0, cantidad que depende de otro índice |
| `gen_procedimientos.gc` | expansión con distintos argumentos, llamadas anidadas, ocultamiento |
| `err_gen_limite_variable.gc` | una variable que vale 500 con `limite_x: 220` |
| `err_gen_feedrate_variable.gc` | `F: vel` con `vel` en 5000 |
| `err_gen_feedrate_cero.gc` | `F: vel` con `vel` en 0 |
| `err_gen_limite_indice.gc` | una variable que se pasa del límite en una vuelta; el error sale una vez |
| `err_gen_proc.gc` | el valor llega por un parámetro, con la nota de la llamada |
| `err_gen_div_cero.gc` | división por una variable que vale 0 |
| `err_gen_sin_cortocircuito.gc` | `x != 0 and 10 / x > 1` con `x` en 0 |

---

## 12. Trabajo futuro (sección del informe LNCS)

Extensiones identificadas y deliberadamente excluidas:

- **Iteración condicional (`while`).** Requiere que el backend deje de ser un desenrollador puro y pase a ser intérprete o generador con saltos simulados.
- **Movimiento relativo con análisis de posición determinada.** Un builtin tipo `MoveRelative(Z: 5)` es implementable manteniendo `G90`: el compilador rastrea la posición absoluta y emite la coordenada calculada. La restricción interesante es que solo sería legal donde la posición es estáticamente determinada — la misma maquinaria de flujo de datos ya usada en el chequeo de la sección 8.1, extendida de "conocida / desconocida" a un valor concreto.
- **Cálculo automático de flujo.** Hoy el programador escribe `E: lado * FLUJO`. El compilador podría derivar el valor de E a partir del diámetro de boquilla, la altura de capa y el diámetro de filamento declarados en el bloque `machine`, quitándole ese cálculo al usuario.
- **Funciones con valor de retorno** y un sistema de tipos más rico.
- **Rangos de temperatura, ventilador y pausa.** Hoy `Fan(S: 300)`, `SetBedTemp(-5)` y `Wait(-1)` compilan: el compilador verifica los límites de los ejes y el feedrate, pero no estos valores, que quedan a cargo del firmware. Verificarlos pediría campos nuevos en la máquina, como las temperaturas máximas.
- **Múltiples perfiles de máquina** con verificación cruzada de portabilidad del mismo programa.

---

## 13. Puntos a consultar con Ruidias

1. Si el alcance (sin `while`, sin funciones con retorno) le parece suficiente, o si espera ver iteración condicional.
2. Si acepta el desenrollado del `repeat` como estrategia de generación, o si prefiere un backend intérprete.
3. Si la decisión de fijar los modos de posicionamiento y extrusión en el compilador le parece una simplificación válida o una pérdida de expresividad.
4. Si el análisis de posición conocida (sección 8.1) le resulta suficiente como aporte de análisis de flujo de datos, o si espera ver algo más.
5. Si le parece bien que el análisis semántico sea conservador aunque el programa no tenga entradas. Como todo valor se conoce al compilar y Marlin no tiene condicionales, el generador evalúa cada `if` y emite una sola rama; el análisis, en cambio, une las dos ramas sin mirar la condición (sección 8.1). Se relaciona con la pregunta 2.
