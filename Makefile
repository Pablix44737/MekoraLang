CC       = gcc
CFLAGS   = -std=c11 -g -Wall
# gnu11 en lugar de c11: el codigo que genera flex usa fileno(), que es POSIX
# y no esta declarado bajo el estandar ISO estricto.
GENFLAGS = -std=gnu11 -g
BIN      = gcodec

SRC   = src
BUILD = build

OBJS = $(BUILD)/parser.tab.o $(BUILD)/lex.yy.o $(BUILD)/main.o $(BUILD)/codegen.o $(BUILD)/errores.o $(BUILD)/tokens.o

.PHONY: all clean test tokens

all: $(BIN)

$(BIN): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

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

$(BUILD):
	mkdir -p $(BUILD)

test: $(BIN)
	./$(BIN) tests/minimo.gc -o $(BUILD)/minimo.gcode
	@echo "--- salida ---"
	@cat $(BUILD)/minimo.gcode

clean:
	rm -rf $(BUILD) $(BIN) salida.gcode

# Prueba del analizador lexico aislado: vuelca los tokens del ejemplo.
tokens: $(BIN)
	./$(BIN) tests/ejemplo.gc --tokens
