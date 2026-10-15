#include "tipos.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *tipo_nombre(TipoDato tipo) {
  switch (tipo) {
  case TIPO_PORCIONES: return "porciones";
  case TIPO_TAZAS: return "tazas";
  case TIPO_BETUN: return "betun";
  case TIPO_CHISPA: return "chispa";
  case TIPO_PUNTO: return "punto";
  case TIPO_NADA: return "nada";
  case TIPO_DESCONOCIDO: break;
  }
  return "desconocido";
}

const char *tipo_equivalente_c(TipoDato tipo) {
  switch (tipo) {
  case TIPO_PORCIONES: return "int";
  case TIPO_TAZAS: return "float";
  case TIPO_BETUN: return "char[]";
  case TIPO_CHISPA: return "char";
  case TIPO_PUNTO: return "bool";
  case TIPO_NADA: return "void";
  case TIPO_DESCONOCIDO: break;
  }
  return "?";
}

TipoDato tipo_desde_texto(const char *texto) {
  if (strcmp(texto, "porciones") == 0) return TIPO_PORCIONES;
  if (strcmp(texto, "tazas") == 0) return TIPO_TAZAS;
  if (strcmp(texto, "betun") == 0) return TIPO_BETUN;
  if (strcmp(texto, "chispa") == 0) return TIPO_CHISPA;
  if (strcmp(texto, "punto") == 0) return TIPO_PUNTO;
  if (strcmp(texto, "nada") == 0) return TIPO_NADA;
  return TIPO_DESCONOCIDO;
}

int tipo_es_numerico(TipoDato tipo) {
  return tipo == TIPO_PORCIONES || tipo == TIPO_TAZAS;
}

char *copiar_texto(const char *texto) {
  size_t largo = strlen(texto);
  char *copia = malloc(largo + 1);
  if (copia == NULL) {
    fprintf(stderr, "Sin memoria\n");
    exit(EXIT_FAILURE);
  }
  memcpy(copia, texto, largo + 1);
  return copia;
}
