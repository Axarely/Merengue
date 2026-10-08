# Makefile del compilador Merengue
#   make          compila
#   make demo     corre la receta de ejemplo mostrando todas las fases
#   make pruebas  corre todos los ejemplos (validos y con errores)
#   make clean    borra lo generado

CC      := gcc
FLEX    := flex
BISON   := bison
CFLAGS  := -Wall -Wextra -std=c11 -O2 -Isrc -Ibuild

OBJETIVO := merengue
BUILD    := build

FUENTES_C := src/main.c src/ast.c src/semantico.c src/tabla_simbolos.c \
             src/interprete.c src/tokens.c src/errores.c src/tipos.c src/formato.c
OBJETOS   := $(patsubst src/%.c,$(BUILD)/%.o,$(FUENTES_C)) \
             $(BUILD)/sintactico.tab.o $(BUILD)/lex.yy.o
CABECERAS := $(wildcard src/*.h)

.PHONY: all demo pruebas clean

all: $(OBJETIVO)

$(OBJETIVO): $(OBJETOS)
	$(CC) $(OBJETOS) -o $@

# Bison: gramatica -> parser en C (+ cabecera con los tokens para Flex)
$(BUILD)/sintactico.tab.c $(BUILD)/sintactico.tab.h: src/sintactico.y | $(BUILD)
	$(BISON) -d -v -Wall -o $(BUILD)/sintactico.tab.c src/sintactico.y

# Flex: patrones de tokens -> lexer en C
$(BUILD)/lex.yy.c: src/lexico.l $(BUILD)/sintactico.tab.h | $(BUILD)
	$(FLEX) -o $@ src/lexico.l

$(BUILD)/sintactico.tab.o: $(BUILD)/sintactico.tab.c $(CABECERAS)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/lex.yy.o: $(BUILD)/lex.yy.c $(CABECERAS)
	$(CC) $(CFLAGS) -D_POSIX_C_SOURCE=200809L -Wno-unused-function -Wno-sign-compare -c $< -o $@

$(BUILD)/%.o: src/%.c $(CABECERAS) $(BUILD)/sintactico.tab.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD):
	mkdir -p $(BUILD)

demo: $(OBJETIVO)
	./$(OBJETIVO) ejemplos/cupcakes.mrg

pruebas: $(OBJETIVO)
	sh pruebas/correr_pruebas.sh

clean:
	rm -rf $(BUILD) $(OBJETIVO)
