#include "errores.h"
#include "tipos.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct {
  int es_advertencia;
  Fase fase;
  int linea;
  char *mensaje;
} Mensaje;

static Mensaje *mensajes;
static int cantidad, capacidad;
static int contador[4];
static int advertencias;

static const char *nombre_fase(Fase fase) {
  switch (fase) {
  case FASE_LEXICA: return "léxico";
  case FASE_SINTACTICA: return "sintáctico";
  case FASE_SEMANTICA: return "semántico";
  case FASE_EJECUCION: return "de ejecución";
  }
  return "";
}

static void guardar(int es_advertencia, Fase fase, int linea, const char *texto) {
  if (cantidad == capacidad) {
    int nueva = capacidad == 0 ? 16 : capacidad * 2;
    Mensaje *mas = realloc(mensajes, (size_t)nueva * sizeof(*mas));
    if (mas == NULL) {
      fprintf(stderr, "Sin memoria\n");
      exit(EXIT_FAILURE);
    }
    mensajes = mas;
    capacidad = nueva;
  }
  mensajes[cantidad].es_advertencia = es_advertencia;
  mensajes[cantidad].fase = fase;
  mensajes[cantidad].linea = linea;
  mensajes[cantidad].mensaje = copiar_texto(texto);
  cantidad++;
}

static void imprimir_mensaje(const Mensaje *m) {
  if (m->es_advertencia) {
    fprintf(stderr, "  [línea %d] Advertencia: %s\n", m->linea, m->mensaje);
  } else if (m->linea > 0) {
    fprintf(stderr, "  [línea %d] Error %s: %s\n", m->linea,
            nombre_fase(m->fase), m->mensaje);
  } else {
    fprintf(stderr, "  Error %s: %s\n", nombre_fase(m->fase), m->mensaje);
  }
}

void reportar_error(Fase fase, int linea, const char *formato, ...) {
  char texto[600];
  va_list argumentos;
  va_start(argumentos, formato);
  vsnprintf(texto, sizeof(texto), formato, argumentos);
  va_end(argumentos);
  contador[fase]++;

  if (fase == FASE_EJECUCION) {
    Mensaje m = {0, fase, linea, texto};
    fflush(stdout);
    imprimir_mensaje(&m);
    fflush(stderr);
    return;
  }
  guardar(0, fase, linea, texto);
}

void reportar_advertencia(int linea, const char *formato, ...) {
  char texto[600];
  va_list argumentos;
  va_start(argumentos, formato);
  vsnprintf(texto, sizeof(texto), formato, argumentos);
  va_end(argumentos);
  advertencias++;
  guardar(1, FASE_SEMANTICA, linea, texto);
}

void errores_imprimir(Fase fase) {
  fflush(stdout);
  for (int i = 0; i < cantidad; i++) {
    if (!mensajes[i].es_advertencia && mensajes[i].fase == fase) {
      imprimir_mensaje(&mensajes[i]);
    }
  }
  fflush(stderr);
}

void advertencias_imprimir(void) {
  fflush(stdout);
  for (int i = 0; i < cantidad; i++) {
    if (mensajes[i].es_advertencia) {
      imprimir_mensaje(&mensajes[i]);
    }
  }
  fflush(stderr);
}

int errores_en(Fase fase) { return contador[fase]; }

int total_advertencias(void) { return advertencias; }

void errores_liberar(void) {
  for (int i = 0; i < cantidad; i++) {
    free(mensajes[i].mensaje);
  }
  free(mensajes);
  mensajes = NULL;
  cantidad = capacidad = 0;
}
