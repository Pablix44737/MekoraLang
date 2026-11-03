# Sentencias del lenguaje

## Tipos de dato

number: número real. Coordenadas, temperaturas, velocidades.
bool: true o false. Solo para condiciones.
machine: una máquina. Solo se usa como tipo de parámetro.

## Declaraciones

### machine

Define la impresora para la que se compila el programa. Es obligatoria, hay una sola y va primera, antes que cualquier otra declaración.

```
machine Ender3 {
    temp_extrusor: 210;
    temp_cama: 60;
    limite_x: 220;
    limite_y: 220;
    limite_z: 250;
    feedrate_max: 3000;
}
```

Hay cuatro campos obligatorios: limite_x, limite_y y limite_z, con los que el compilador verifica que ningún movimiento se salga de la cama, y feedrate_max, con el que verifica que ninguna velocidad F supere la de la máquina. El resto de los campos los elige el programador. Se accede a un campo con punto: Ender3.limite_x

### const

Un valor con nombre que no cambia. Va fuera de los procedimientos.

```
const MARGEN : number = 10;
```

### procedure

Agrupa sentencias bajo un nombre. Puede recibir parámetros y no devuelve ningún valor.

```
procedure Preparar(m: machine) {
    SetBedTemp(m.temp_cama);
    AutoHome();
}
```

Se invoca por su nombre: Preparar(Ender3);

Los parámetros son de solo lectura. Un procedimiento solo puede llamar a los que están declarados más arriba en el archivo, y no puede llamarse a sí mismo: el compilador copia el cuerpo de cada procedimiento en el lugar de la llamada, y una llamada recursiva no terminaría nunca.

### main

Punto de entrada del programa. Hay uno solo y es lo único que se ejecuta.

```
main {
    Preparar(Ender3);
}
```

## Sentencias

### var

Declara una variable local al bloque. Se inicializa siempre.

```
var vel : number = 1500;
```

Un bloque interno puede declarar un nombre que ya existe afuera; dentro del bloque vale el de adentro.

### Asignación

```
vel = 3000;
```

### if / else

Las llaves son obligatorias aunque haya una sola sentencia. La condición debe ser de tipo bool.

```
if (vel > 3000) {
    vel = 3000;
} else {
    Message("Velocidad OK");
}
```

Se pueden encadenar con else if.

### repeat

Repite un bloque una cantidad fija de veces. El número tiene que ser conocido al compilar, porque el compilador desarma el bucle al generar el G-code.

```
repeat 5 {
    Extrude(E: 10, F: 300);
}
```

Con as se obtiene el número de vuelta, empezando en 0, para variar algo en cada repetición. Esa variable es de solo lectura.

```
repeat 4 as fila {
    Move(X: 10, Y: 10 + fila * 50, F: 3000);
}
```

### Comentarios

```
// desde las dos barras hasta el fin de línea
```

## Operadores

Aritméticos: + - * / %
Comparación: == != < > <= >=
Lógicos: and or not

Precedencia, de mayor a menor: menos unario, luego * / %, luego + -, luego comparación, luego not, luego and, luego or. Por eso `not a > 3` significa `not (a > 3)`. Las comparaciones no se encadenan: `a < b < c` es un error.

## Comandos de la máquina

Movimiento y material:

Move(X:, Y:, Z:, F:) desplaza el cabezal sin depositar material. Los ejes son opcionales pero tiene que haber al menos uno. F es la velocidad.

Draw(X:, Y:, Z:, E:, F:) desplaza el cabezal depositando material. E son los milímetros de filamento a extruir durante el recorrido.

Extrude(E:, F:) empuja filamento sin mover el cabezal. Para purgar o cargar material.

AutoHome() lleva los tres ejes contra los finales de carrera. AutoHome(X, Y) referencia solo los ejes nombrados.

Temperatura:

SetExtruderTemp(n) enciende el extrusor a n grados y sigue sin esperar.
WaitExtruderTemp(n) enciende el extrusor a n grados y espera a que llegue.
SetBedTemp(n) enciende la cama a n grados sin esperar.
WaitBedTemp(n) enciende la cama a n grados y espera.
TurnOffHeaters() apaga extrusor y cama.

Conviene encender los dos con Set y después esperar los dos con Wait, así calientan en paralelo.

Control y mantenimiento:

Wait(segundos) pausa la ejecución.
Fan(S:) fija la velocidad del ventilador, de 0 a 255. Con 0 lo apaga.
DisableMotors() libera los motores para poder mover la máquina a mano.
Message("texto") muestra un mensaje en la pantalla de la impresora. El texto no puede tener punto y coma, porque en G-code empieza un comentario.
Raw("...") emite una línea de G-code tal cual, para comandos que el lenguaje no cubre.

## Reglas a tener en cuenta

Hay que llamar a AutoHome() antes del primer Move o Draw. Al encender, la impresora no sabe dónde está el cabezal y el compilador rechaza cualquier movimiento hasta que se referencien los ejes.

Después de DisableMotors() hace falta volver a llamar a AutoHome() antes de mover. Con los motores liberados el operario pudo haber corrido la máquina a mano, así que la posición anterior ya no vale.

Después de un if, la posición se considera conocida solo si lo es al final de todas las ramas, sin importar cuál se vaya a tomar.

Las coordenadas de Move y Draw tienen que estar entre 0 y el límite de la máquina, y F no puede superar feedrate_max. Si el valor depende de una variable, el análisis no lo puede verificar y avisa con una advertencia. Después, al generar el G-code, el compilador ya conoce el valor real: si se pasa, es un error y no se genera el archivo.

## Lo que agrega el compilador

No hace falta escribirlo. Al principio del programa se fijan milímetros como unidad, coordenadas absolutas y extrusión relativa. Por eso no hay ninguna sentencia para cambiar de modo: las coordenadas de Move y Draw son siempre posiciones absolutas de la cama, y el E de Extrude y Draw es siempre cuánto filamento más, nunca un acumulado.

Al final del programa se apagan el extrusor, la cama, el ventilador y los motores, aunque el programa no lo pida.

## Ejemplo

Rutina de mantenimiento. Calienta, purga la boquilla, recorre la cama para verificar los ejes y deja la máquina lista para limpiar a mano.

```
machine Ender3 {
    temp_extrusor: 210;
    temp_cama: 60;
    limite_x: 220;
    limite_y: 220;
    limite_z: 250;
    feedrate_max: 3000;
}

const MARGEN : number = 10;

procedure Preparar(m: machine) {
    SetBedTemp(m.temp_cama);
    SetExtruderTemp(m.temp_extrusor);
    WaitBedTemp(m.temp_cama);
    WaitExtruderTemp(m.temp_extrusor);
    AutoHome();
}

procedure PurgarBoquilla() {
    Message("Purgando");
    repeat 5 {
        Extrude(E: 10, F: 300);
        Wait(2);
    }
}

procedure BarridoCama(m: machine) {
    var vel : number = m.feedrate_max;

    if (vel > 3000) {
        vel = 3000;
    }

    repeat 4 as fila {
        Move(X: MARGEN,              Y: MARGEN + fila * 50, F: vel);
        Move(X: m.limite_x - MARGEN, Y: MARGEN + fila * 50, F: vel);
    }
}

procedure LiberarParaLimpieza() {
    TurnOffHeaters();
    Message("Listo - mover a mano");
    DisableMotors();
}

main {
    Preparar(Ender3);
    PurgarBoquilla();
    BarridoCama(Ender3);
    LiberarParaLimpieza();
}
```
