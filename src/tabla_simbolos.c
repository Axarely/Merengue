#include "tabla_simbolos.h"
#include "formato.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void tabla_iniciar(TablaSimbolos *tabla) {
  tabla->simbolos = NULL;
  tabla->cantidad = 0;
  tabla->capacidad = 0;
}

Simbolo *tabla_buscar(TablaSimbolos *tabla, const char *nombre) {
  for (int i = 0; i < tabla->cantidad; i++) {
    if (strcmp(tabla->simbolos[i].nombre, nombre) == 0) {
      return &tabla->simbolos[i];
    }
  }
  return NULL;
}

Simbolo *tabla_insertar(TablaSimbolos *tabla, const char *nombre, TipoDato tipo,
                        int es_secreto, int linea, int tiene_valor_inicial) {
  if (tabla_buscar(tabla, nombre) != NULL) {
    return NULL; /* "Ese molde ya existe" */
  }
  if (tabla->cantidad == tabla->capacidad) {
    int nueva = tabla->capacidad == 0 ? 16 : tabla->capacidad * 2;
    Simbolo *mas = realloc(tabla->simbolos, (size_t)nueva * sizeof(*mas));
    if (mas == NULL) {
      fprintf(stderr, "Sin memoria\n");
      exit(EXIT_FAILURE);
    }
    tabla->simbolos = mas;
    tabla->capacidad = nueva;
  }
  Simbolo *simbolo = &tabla->simbolos[tabla->cantidad++];
  simbolo->nombre = copiar_texto(nombre);
  simbolo->tipo = tipo;
  simbolo->es_secreto = es_secreto;
  simbolo->linea_declaracion = linea;
  simbolo->tiene_valor_inicial = tiene_valor_inicial;
  simbolo->usos = 0;
  simbolo->modificaciones = 0;
  simbolo->valor = valor_por_defecto(tipo);
  return simbolo;
}

void tabla_imprimir(const TablaSimbolos *tabla, int con_valores) {
  enum { COLUMNAS = 9 };
  const int anchos[COLUMNAS] = {16, 9, 6, 9, 5, 13, 4, 7, 22};
  const char *encabezado[COLUMNAS] = {"Nombre", "Tipo",  "En C",
                                      "Categoría", "Línea", "Valor inicial",
                                      "Usos", "Cambios", "Valor final"};
  int columnas = con_valores ? COLUMNAS : COLUMNAS - 1;

  if (tabla->cantidad == 0) {
    printf("  (la mesa está vacía: no se declaró ningún molde)\n");
    return;
  }

  imprimir_separador(anchos, columnas);
  imprimir_fila(encabezado, anchos, columnas);
  imprimir_separador(anchos, columnas);
  for (int i = 0; i < tabla->cantidad; i++) {
    const Simbolo *s = &tabla->simbolos[i];
    char linea[16], usos[16], cambios[16], final[128];
    snprintf(linea, sizeof(linea), "%d", s->linea_declaracion);
    snprintf(usos, sizeof(usos), "%d", s->usos);
    snprintf(cambios, sizeof(cambios), "%d", s->modificaciones);
    valor_a_texto(&s->valor, final, sizeof(final));
    const char *celdas[COLUMNAS] = {
        s->nombre,
        tipo_nombre(s->tipo),
        tipo_equivalente_c(s->tipo),
        s->es_secreto ? "secreto" : "molde",
        linea,
        s->tiene_valor_inicial ? "con valor" : "por defecto",
        usos,
        cambios,
        final};
    imprimir_fila(celdas, anchos, columnas);
  }
  imprimir_separador(anchos, columnas);
}

void tabla_liberar(TablaSimbolos *tabla) {
  for (int i = 0; i < tabla->cantidad; i++) {
    free(tabla->simbolos[i].nombre);
    valor_liberar(&tabla->simbolos[i].valor);
  }
  free(tabla->simbolos);
  tabla_iniciar(tabla);
}

/* ------------------------------------------------------------------ */
/* Valores                                                             */
/* ------------------------------------------------------------------ */

Valor valor_por_defecto(TipoDato tipo) {
  Valor valor;
  memset(&valor, 0, sizeof(valor));
  valor.tipo = tipo;
  valor.chispa = ' ';
  if (tipo == TIPO_BETUN) {
    valor.betun = copiar_texto("");
  }
  return valor;
}

void valor_a_texto(const Valor *valor, char *destino, int tamano) {
  switch (valor->tipo) {
  case TIPO_PORCIONES:
    snprintf(destino, (size_t)tamano, "%d", valor->porciones);
    break;
  case TIPO_TAZAS:
    formatear_tazas(valor->tazas, destino, tamano);
    break;
  case TIPO_BETUN:
    snprintf(destino, (size_t)tamano, "\"%s\"", valor->betun ? valor->betun : "");
    break;
  case TIPO_CHISPA:
    snprintf(destino, (size_t)tamano, "'%c'", valor->chispa);
    break;
  case TIPO_PUNTO:
    snprintf(destino, (size_t)tamano, "%s", valor->punto ? "dorado" : "crudo");
    break;
  case TIPO_DESCONOCIDO:
    snprintf(destino, (size_t)tamano, "?");
    break;
  }
}

void valor_asignar(Valor *destino, const Valor *origen) {
  char *betun_nuevo = NULL;
  if (origen->tipo == TIPO_BETUN) {
    betun_nuevo = copiar_texto(origen->betun ? origen->betun : "");
  }
  valor_liberar(destino);
  *destino = *origen;
  destino->betun = betun_nuevo;
}

void valor_liberar(Valor *valor) {
  free(valor->betun);
  valor->betun = NULL;
}
