#include "formato.h"

#include <stdio.h>
#include <string.h>

/* Cuenta caracteres visibles de un texto UTF-8 (una 'a' con tilde = 1). */
static int columnas_visibles(const char *texto) {
  int total = 0;
  for (const unsigned char *c = (const unsigned char *)texto; *c; c++) {
    if ((*c & 0xC0) != 0x80) {
      total++;
    }
  }
  return total;
}

void imprimir_celda(const char *texto, int ancho) {
  int visibles = columnas_visibles(texto);
  if (visibles > ancho) {
    /* Recorta textos muy largos para no romper la tabla. */
    int mostrados = 0;
    const unsigned char *c = (const unsigned char *)texto;
    while (*c && mostrados < ancho - 3) {
      putchar(*c);
      c++;
      while ((*c & 0xC0) == 0x80) {
        putchar(*c);
        c++;
      }
      mostrados++;
    }
    printf("...");
    return;
  }
  printf("%s", texto);
  for (int i = visibles; i < ancho; i++) {
    putchar(' ');
  }
}

void imprimir_separador(const int *anchos, int columnas) {
  putchar('+');
  for (int i = 0; i < columnas; i++) {
    for (int j = 0; j < anchos[i] + 2; j++) {
      putchar('-');
    }
    putchar('+');
  }
  putchar('\n');
}

void imprimir_fila(const char **celdas, const int *anchos, int columnas) {
  putchar('|');
  for (int i = 0; i < columnas; i++) {
    putchar(' ');
    imprimir_celda(celdas[i], anchos[i]);
    printf(" |");
  }
  putchar('\n');
}

void formatear_tazas(double valor, char *destino, int tamano) {
  snprintf(destino, (size_t)tamano, "%.6g", valor);
  if (strchr(destino, '.') == NULL && strchr(destino, 'e') == NULL) {
    /* 3 -> 3,0 para que se note que es tazas y no porciones */
    strncat(destino, ".0", (size_t)tamano - strlen(destino) - 1);
  }
  for (char *c = destino; *c; c++) {
    if (*c == '.') {
      *c = ',';
    }
  }
}

void imprimir_titulo(const char *titulo) {
  printf("\n");
  printf("==================================================================\n");
  printf("  %s\n", titulo);
  printf("==================================================================\n");
}
