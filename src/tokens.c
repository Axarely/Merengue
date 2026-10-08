#include "tokens.h"
#include "formato.h"
#include "tipos.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  int linea;
  const char *categoria; /* textos fijos, no se liberan */
  char *lexema;
} Token;

static Token *lista;
static int cantidad;
static int capacidad;

void tokens_registrar(int linea, const char *categoria, const char *lexema) {
  if (cantidad == capacidad) {
    int nueva = capacidad == 0 ? 64 : capacidad * 2;
    Token *mas = realloc(lista, (size_t)nueva * sizeof(*mas));
    if (mas == NULL) {
      fprintf(stderr, "Sin memoria\n");
      exit(EXIT_FAILURE);
    }
    lista = mas;
    capacidad = nueva;
  }
  lista[cantidad].linea = linea;
  lista[cantidad].categoria = categoria;
  lista[cantidad].lexema = copiar_texto(lexema);
  cantidad++;
}

int tokens_total(void) { return cantidad; }

void tokens_imprimir(void) {
  enum { COLUMNAS = 4 };
  const int anchos[COLUMNAS] = {3, 5, 22, 30};
  const char *encabezado[COLUMNAS] = {"#", "Línea", "Categoría", "Lexema"};

  imprimir_separador(anchos, COLUMNAS);
  imprimir_fila(encabezado, anchos, COLUMNAS);
  imprimir_separador(anchos, COLUMNAS);
  for (int i = 0; i < cantidad; i++) {
    char numero[16], linea[16];
    snprintf(numero, sizeof(numero), "%d", i + 1);
    snprintf(linea, sizeof(linea), "%d", lista[i].linea);
    const char *celdas[COLUMNAS] = {numero, linea, lista[i].categoria,
                                    lista[i].lexema};
    imprimir_fila(celdas, anchos, COLUMNAS);
  }
  imprimir_separador(anchos, COLUMNAS);
}

void tokens_liberar(void) {
  for (int i = 0; i < cantidad; i++) {
    free(lista[i].lexema);
  }
  free(lista);
  lista = NULL;
  cantidad = capacidad = 0;
}

static char resolver_escape(char c) {
  switch (c) {
  case 'n': return '\n';
  case 't': return '\t';
  case '0': return '\0';
  default: return c; /* \\  \'  \" */
  }
}

char chispa_desde_texto(const char *texto) {
  /* texto viene con comillas: 'A'  o  '\n' */
  if (texto[1] == '\\') {
    return resolver_escape(texto[2]);
  }
  return texto[1];
}

char *betun_desde_texto(const char *texto) {
  size_t largo = strlen(texto);
  char *resultado = malloc(largo + 1);
  if (resultado == NULL) {
    fprintf(stderr, "Sin memoria\n");
    exit(EXIT_FAILURE);
  }
  size_t j = 0;
  /* Se salta la comilla inicial y la final. */
  for (size_t i = 1; i + 1 < largo; i++) {
    if (texto[i] == '\\' && i + 2 < largo) {
      i++;
      resultado[j++] = resolver_escape(texto[i]);
    } else {
      resultado[j++] = texto[i];
    }
  }
  resultado[j] = '\0';
  return resultado;
}
