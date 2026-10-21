CC       = gcc
# -MMD -MP: gcc anota en un .d de que headers depende cada .o, y make lo
# lee al final del archivo. Asi, si cambia un .h, se recompila lo que lo usa.
CFLAGS   = -std=c11 -g -Wall -MMD -MP
# gnu11 en lugar de c11: el codigo que genera flex usa fileno(), que es POSIX
# y no esta declarado bajo el estandar ISO estricto.
GENFLAGS = -std=gnu11 -g -MMD -MP
BIN      = gcodec
# fmod y floor, para evaluar expresiones en compilacion. En Cygwin y MSYS2
# estan en la biblioteca de C, pero en Linux hace falta -lm.
LDLIBS   = -lm

SRC   = src
BUILD = build

OBJS = $(BUILD)/parser.tab.o $(BUILD)/lex.yy.o $(BUILD)/main.o $(BUILD)/codegen.o $(BUILD)/errores.o $(BUILD)/tokens.o \
       $(BUILD)/ast.o $(BUILD)/simbolos.o $(BUILD)/builtins.o $(BUILD)/evaluador.o \
       $(BUILD)/semantico.o $(BUILD)/posicion.o

.PHONY: all clean test tokens ast tabla

all: $(BIN)

$(BIN): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDLIBS)

# Bison genera el .c y, con -d, el .h con el enum de tokens que usa el lexer.
$(BUILD)/parser.tab.c $(BUILD)/parser.tab.h: $(SRC)/parser.y | $(BUILD)
	bison -d -o $(BUILD)/parser.tab.c $(SRC)/parser.y

# El lexer depende del header de bison: sin el no conoce los tokens.
$(BUILD)/lex.yy.c: $(SRC)/lexer.l $(BUILD)/parser.tab.h | $(BUILD)
	flex -o $@ $(SRC)/lexer.l

# El codigo generado por flex y bison se compila sin -Wall para no
# ensuciar la salida con advertencias que no son nuestras.
$(BUILD)/parser.tab.o: $(BUILD)/parser.tab.c $(BUILD)/parser.tab.h
	$(CC) $(GENFLAGS) -I$(SRC) -I$(BUILD) -c -o $@ $<

$(BUILD)/lex.yy.o: $(BUILD)/lex.yy.c $(BUILD)/parser.tab.h
	$(CC) $(GENFLAGS) -I$(SRC) -I$(BUILD) -c -o $@ $<

$(BUILD)/%.o: $(SRC)/%.c | $(BUILD)
	$(CC) $(CFLAGS) -I$(SRC) -I$(BUILD) -c -o $@ $<

# tokens.c incluye parser.tab.h. En el primer build todavia no hay .d que
# lo diga, asi que la dependencia se declara a mano para que bison corra antes.
$(BUILD)/tokens.o: $(BUILD)/parser.tab.h

$(BUILD):
	mkdir -p $(BUILD)

test: $(BIN)
	./$(BIN) tests/minimo.gc -o $(BUILD)/minimo.gcode
	@echo "--- salida ---"
	@cat $(BUILD)/minimo.gcode

clean:
	rm -rf $(BUILD) $(BIN) salida.gcode

# Programa que usan "make tokens", "make ast" y "make tabla". Se cambia
# desde la linea de comandos: make ast PROG=tests/sintaxis.gc
PROG ?= tests/ejemplo.gc

# Prueba del analizador lexico aislado: vuelca los tokens.
tokens: $(BIN)
	./$(BIN) $(PROG) --tokens

# Prueba del analizador sintactico: imprime el arbol.
ast: $(BIN)
	./$(BIN) $(PROG) --ast

# Prueba del analisis semantico: imprime la tabla de simbolos por alcance.
tabla: $(BIN)
	./$(BIN) $(PROG) --tabla

-include $(wildcard $(BUILD)/*.d)
