#!/bin/bash
# correr_pruebas.sh - corre todos los programas de tests/ y los compara con
# lo que dice su encabezado "// Esperado: ...".
#
# Uso: bash tests/correr_pruebas.sh [ruta a gcodec]
# Normalmente se llama con "make pruebas". Devuelve 0 si pasan todas.

GCODEC=${1:-./gcodec}
DIR=build/pruebas
SALIDA=$DIR/salida.gcode
MENSAJES=$DIR/mensajes.txt
mkdir -p "$DIR"

pasan=0
fallan=0

falla() {
    echo "FALLA $nombre: $1"
    sed 's/^/        /' "$MENSAJES"
    fallan=$((fallan + 1))
}

# errores (o advertencias) reportados en una linea: hay "error sintactico" 12
hay() {
    grep -qE "^[^:]+:$2:[0-9]+: $1" "$MENSAJES"
}

cuantos_errores() {
    grep -cE "^[^:]+:[0-9]+:[0-9]+: error" "$MENSAJES"
}

for prog in tests/*.gc; do
    nombre=$(basename "$prog" .gc)
    esperado=$(grep -m1 '// Esperado:' "$prog" | sed 's/.*Esperado: //' | tr -d '\r')
    linea=$(echo "$esperado" | sed -nE 's/^[^0-9]*([0-9]+).*/\1/p')

    rm -f "$SALIDA"
    "$GCODEC" "$prog" -o "$SALIDA" > "$MENSAJES" 2>&1
    codigo=$?
    tr -d '\r' < "$MENSAJES" > "$MENSAJES.tmp" && mv "$MENSAJES.tmp" "$MENSAJES"

    case "$esperado" in
    "compila sin errores"*)
        if [ $codigo -ne 0 ]; then
            falla "tenia que compilar y devolvio $codigo"; continue
        fi
        # lineas con advertencia: las que nombra el encabezado y ninguna mas
        lineas=$(echo "$esperado" | sed -nE 's/.*advertencias? en las? lineas? (.*)/\1/p' | grep -oE '[0-9]+')
        reales=$(grep -oE "^[^:]+:[0-9]+:[0-9]+: advertencia" "$MENSAJES" | cut -d: -f2 | sort -un)
        if [ "$(echo $lineas)" != "$(echo $reales)" ]; then
            falla "advertencias en las lineas [$(echo $reales)], se esperaban en [$(echo $lineas)]"; continue
        fi
        if [ ! -f "tests/esperado/$nombre.gcode" ]; then
            falla "falta tests/esperado/$nombre.gcode"; continue
        fi
        if ! diff <(tr -d '\r' < "$SALIDA") "tests/esperado/$nombre.gcode" > "$MENSAJES"; then
            falla "el G-code no coincide con tests/esperado/$nombre.gcode"; continue
        fi
        ;;

    "error en la linea "* | "error sintactico en la linea "*)
        clase="error"
        case "$esperado" in "error sintactico"*) clase="error sintactico" ;; esac
        fragmento=$(echo "$esperado" | sed -E 's/^[^:]*: //; s/ \(nota en la linea [0-9]+\)$//')
        nota=$(echo "$esperado" | sed -nE 's/.*\(nota en la linea ([0-9]+)\)$/\1/p')
        if [ $codigo -ne 1 ]; then
            falla "tenia que fallar y devolvio $codigo"; continue
        fi
        # un parentesis al final, como "(con fila = 4)", puede estar mas
        # adelante en el mensaje
        aparte=$(echo "$fragmento" | sed -nE 's/.* (\([^()]*\))$/\1/p')
        sin_aparte=${fragmento% "$aparte"}
        en_linea=$(grep -E "^[^:]+:$linea:[0-9]+: $clase" "$MENSAJES")
        if ! echo "$en_linea" | grep -qF -- "$fragmento" &&
           ! { [ -n "$aparte" ] && echo "$en_linea" | grep -F -- "$sin_aparte" | grep -qF -- "$aparte"; }; then
            falla "no aparece en la linea $linea: $fragmento"; continue
        fi
        if [ -n "$nota" ] && ! hay "nota" "$nota"; then
            falla "falta la nota en la linea $nota"; continue
        fi
        if [ -f "$SALIDA" ]; then
            falla "fallo pero dejo el archivo de salida"; continue
        fi
        ;;

    "tres errores en la linea "*)
        if [ $codigo -ne 1 ] || [ "$(cuantos_errores)" -ne 3 ] ||
           [ "$(grep -cE "^[^:]+:$linea:[0-9]+: error" "$MENSAJES")" -ne 3 ]; then
            falla "se esperaban tres errores en la linea $linea"; continue
        fi
        ;;

    "")
        # los tests lexicos y sintacticos del paso 3 no tienen encabezado:
        # alcanza con que fallen con algun error
        if [ $codigo -ne 1 ] || [ "$(cuantos_errores)" -eq 0 ]; then
            falla "tenia que fallar con un error y devolvio $codigo"; continue
        fi
        ;;

    *)
        falla "no entiendo el encabezado: $esperado"; continue
        ;;
    esac

    echo "ok    $nombre"
    pasan=$((pasan + 1))
done

echo
echo "$((pasan + fallan)) pruebas: $pasan pasan, $fallan fallan"
[ $fallan -eq 0 ]
