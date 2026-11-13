CC       = gcc
# -MMD -MP: dependencias de los .h
CFLAGS   = -std=c11 -g -Wall -MMD -MP
# gnu11 porque flex usa fileno()
GENFLAGS = -std=gnu11 -g -MMD -MP
BIN      = gcodec
# fmod y floor (en Linux hace falta -lm)
LDLIBS   = -lm

SRC   = src
BUILD = build

OBJS = $(BUILD)/parser.tab.o $(BUILD)/lex.yy.o $(BUILD)/main.o $(BUILD)/codegen.o $(BUILD)/errores.o $(BUILD)/tokens.o \
       $(BUILD)/ast.o $(BUILD)/simbolos.o $(BUILD)/builtins.o $(BUILD)/evaluador.o \
       $(BUILD)/semantico.o $(BUILD)/posicion.o $(BUILD)/recorrido.o

.PHONY: all clean test pruebas tokens ast tabla

all: $(BIN)

$(BIN): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDLIBS)

# bison -d genera tambien el .h de los tokens
$(BUILD)/parser.tab.c $(BUILD)/parser.tab.h: $(SRC)/parser.y | $(BUILD)
	bison -d -o $(BUILD)/parser.tab.c $(SRC)/parser.y

# el lexer necesita parser.tab.h
$(BUILD)/lex.yy.c: $(SRC)/lexer.l $(BUILD)/parser.tab.h | $(BUILD)
	flex -o $@ $(SRC)/lexer.l

# lo generado por flex y bison, sin -Wall
$(BUILD)/parser.tab.o: $(BUILD)/parser.tab.c $(BUILD)/parser.tab.h
	$(CC) $(GENFLAGS) -I$(SRC) -I$(BUILD) -c -o $@ $<

$(BUILD)/lex.yy.o: $(BUILD)/lex.yy.c $(BUILD)/parser.tab.h
	$(CC) $(GENFLAGS) -I$(SRC) -I$(BUILD) -c -o $@ $<

$(BUILD)/%.o: $(SRC)/%.c | $(BUILD)
	$(CC) $(CFLAGS) -I$(SRC) -I$(BUILD) -c -o $@ $<

# tokens.c incluye parser.tab.h: bison tiene que correr antes
$(BUILD)/tokens.o: $(BUILD)/parser.tab.h

$(BUILD):
	mkdir -p $(BUILD)

test: $(BIN)
	./$(BIN) tests/minimo.gc -o $(BUILD)/minimo.gcode
	@echo "--- salida ---"
	@cat $(BUILD)/minimo.gcode

# Corre todos los programas de tests/ y los compara con lo esperado.
pruebas: $(BIN)
	bash tests/correr_pruebas.sh ./$(BIN)

clean:
	rm -rf $(BUILD) $(BIN) salida.gcode

# programa para make tokens/ast/tabla, ej.: make ast PROG=tests/sintaxis.gc
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
