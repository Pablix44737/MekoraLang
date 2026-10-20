# Especificación del lenguaje (v4) — DSL de alto nivel → G-code

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

Tres tipos: `number` (real, se emite con 3 decimales), `bool`, `machine`.

- No hay tipo `string`: las cadenas existen únicamente como literal argumento de builtins (`Message`, `Raw`).
- No hay conversión implícita entre `number` y `bool`.
- El tipo `machine` solo aparece como tipo de parámetro; no se puede declarar una variable local de tipo `machine` ni asignar una máquina.
- Los campos de un bloque `machine` son siempre `number`.

---

## 6. Modos fijos: prólogo y epílogo automáticos

**El programador no elige ni puede cambiar el modo de posicionamiento ni el de extrusión.** El compilador los fija en un prólogo que emite siempre:

```gcode
; Generado automáticamente — no editar
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

`DisableMotors()` es una operación de mantenimiento genuina, no azúcar del epílogo: con los steppers energizados los ejes quedan retenidos y no se pueden mover a mano. Liberarlos es requisito para manipular la máquina físicamente. Ver la sección 8.1, porque tiene una consecuencia semántica importante.

### Builtins descartados y por qué

- **`Rapid()` (G0)** — En Marlin es sinónimo de `G1`; "desplazarse rápido" no es una operación distinta del dominio. Si se quisiera el guiño de legibilidad de los slicers, el compilador puede derivarlo solo (`Move` → `G0`, `Draw` → `G1`) con una condición en el generador y cero sentencias nuevas en el lenguaje. Es puramente cosmético.
- **`SetAbsolute()` / `SetRelative()`** — Los modos son fijos (sección 6).
- **`ReportPosition()` (M114), `ReportTemps()` (M105), `ReadEndstops()` (M119)** — Son comandos de *consulta*: responden por el puerto serie al host que los envió. Solo tienen efecto observable desde OctoPrint, Pronterface o similar conectado por USB; si el `.gcode` se imprime desde la tarjeta SD, la respuesta no la lee nadie. Quien los necesite los emite con `Raw("M119")`.

### Nota de implementación

Los builtins deben implementarse como una **tabla de datos** (nombre, parámetros admitidos con su tipo y obligatoriedad, plantilla de G-code), no como un `case` por builtin en el generador. Con esa arquitectura el chequeo de aridad y tipos recorre la tabla, y agregar un builtin cuesta una fila.

---

## 8. Análisis semántico

### 8.1 Validez de la posición (el chequeo distintivo del compilador)

El compilador mantiene, por cada eje, un booleano **"posición conocida"**. Tres booleanos en total.

- **Al inicio del programa los tres están en falso.** Una impresora recién encendida no sabe dónde está.
- **`AutoHome()`** los pone en verdadero. `AutoHome(X:, Y:)` pone en verdadero solo los ejes nombrados.
- **`DisableMotors()`** los pone todos en falso: liberados los motores, el operario puede mover la máquina a mano y la posición real deja de ser la que el compilador cree.
- **`Move` o `Draw` sobre un eje cuya posición no es conocida es un error de compilación.**

En un `if`, la posición de un eje queda en verdadero después del condicional solo si lo está en **todas** las ramas (es el join conservador de un análisis de flujo de datos; en el informe conviene nombrarlo así).

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
14. **Límites físicos** (abajo).

### 8.3 Validación de límites

Con el posicionamiento fijado en absoluto, el argumento de cada `Move` / `Draw` es directamente la coordenada destino:

- Si el argumento es una **expresión constante** (literales, constantes, campos de máquina, y el índice de `repeat` una vez desenrollado): se evalúa y se emite **error de compilación** si queda fuera de `[0, limite_eje]`.
- Si depende de una variable no determinable estáticamente: **advertencia**, y se emite un comentario en el G-code generado.

Como el `repeat` se desenrolla por completo, la enorme mayoría de los movimientos cae en el primer caso.

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
; Generado automáticamente — no editar
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

---

## 11. Escenarios de prueba (requisito de la cátedra)

Casos válidos: programa mínimo (solo `main`), máquina con un único campo, procedimiento sin parámetros, `if` sin `else`, `if/else if/else` encadenado, `repeat` anidado, `repeat` con índice, expresión aritmética con precedencia mixta, `Draw` completo, `DisableMotors()` seguido de `AutoHome()` y movimiento, uso de `Raw`.

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

---

## 12. Trabajo futuro (sección del informe LNCS)

Extensiones identificadas y deliberadamente excluidas:

- **Iteración condicional (`while`).** Requiere que el backend deje de ser un desenrollador puro y pase a ser intérprete o generador con saltos simulados.
- **Movimiento relativo con análisis de posición determinada.** Un builtin tipo `MoveRelative(Z: 5)` es implementable manteniendo `G90`: el compilador rastrea la posición absoluta y emite la coordenada calculada. La restricción interesante es que solo sería legal donde la posición es estáticamente determinada — la misma maquinaria de flujo de datos ya usada en el chequeo de la sección 8.1, extendida de "conocida / desconocida" a un valor concreto.
- **Cálculo automático de flujo.** Hoy el programador escribe `E: lado * FLUJO`. El compilador podría derivar el valor de E a partir del diámetro de boquilla, la altura de capa y el diámetro de filamento declarados en el bloque `machine`, quitándole ese cálculo al usuario.
- **Funciones con valor de retorno** y un sistema de tipos más rico.
- **Múltiples perfiles de máquina** con verificación cruzada de portabilidad del mismo programa.

---

## 13. Puntos a consultar con Ruidias

1. Si el alcance (sin `while`, sin funciones con retorno) le parece suficiente, o si espera ver iteración condicional.
2. Si acepta el desenrollado del `repeat` como estrategia de generación, o si prefiere un backend intérprete.
3. Si la decisión de fijar los modos de posicionamiento y extrusión en el compilador le parece una simplificación válida o una pérdida de expresividad.
4. Si el análisis de posición conocida (sección 8.1) le resulta suficiente como aporte de análisis de flujo de datos, o si espera ver algo más.
