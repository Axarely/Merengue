#include "tabla_simbolos.h"
#include "formato.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void tabla_iniciar(TablaSimbolos *tabla) {
  tabla->simbolos = NULL;
  tabla->cantidad = 0;
  tabla->capacidad = 0;
  tabla->nivel_actual = 0;
  tabla->ambito_actual = NULL;
}

/* ------------------------------------------------------------------ */
/* Ambitos                                                             */
/* ------------------------------------------------------------------ */

static const char *ambito_texto(const TablaSimbolos *tabla) {
  return tabla->ambito_actual != NULL ? tabla->ambito_actual : "global";
}

void tabla_entrar_ambito(TablaSimbolos *tabla, const char *nombre_preparacion) {
  free(tabla->ambito_actual);
  tabla->ambito_actual = copiar_texto(nombre_preparacion);
  tabla->nivel_actual = 1;
}

void tabla_salir_ambito(TablaSimbolos *tabla) {
  /* Los nombres de la preparacion dejan de ser visibles, pero se quedan
     en la tabla para poder mostrarlos. */
  for (int i = 0; i < tabla->cantidad; i++) {
    if (tabla->simbolos[i].nivel == tabla->nivel_actual &&
        tabla->simbolos[i].activo) {
      tabla->simbolos[i].activo = 0;
    }
  }
  free(tabla->ambito_actual);
  tabla->ambito_actual = NULL;
  tabla->nivel_actual = 0;
}

static int buscar_en_nivel(const TablaSimbolos *tabla, const char *nombre,
                           int nivel) {
  for (int i = 0; i < tabla->cantidad; i++) {
    const Simbolo *s = &tabla->simbolos[i];
    if (s->activo && s->nivel == nivel && strcmp(s->nombre, nombre) == 0) {
      return i;
    }
  }
  return -1;
}

int tabla_buscar_local(const TablaSimbolos *tabla, const char *nombre) {
  return buscar_en_nivel(tabla, nombre, tabla->nivel_actual);
}

int tabla_buscar_global(const TablaSimbolos *tabla, const char *nombre) {
  return buscar_en_nivel(tabla, nombre, 0);
}

int tabla_buscar(const TablaSimbolos *tabla, const char *nombre) {
  int posicion = tabla_buscar_local(tabla, nombre);
  if (posicion < 0 && tabla->nivel_actual > 0) {
    posicion = tabla_buscar_global(tabla, nombre);
  }
  return posicion;
}

Simbolo *tabla_simbolo(TablaSimbolos *tabla, int posicion) {
  if (posicion < 0 || posicion >= tabla->cantidad) {
    return NULL;
  }
  return &tabla->simbolos[posicion];
}

int tabla_insertar(TablaSimbolos *tabla, const char *nombre, TipoDato tipo,
                   Categoria categoria, int linea, int tiene_valor_inicial) {
  if (tabla_buscar_local(tabla, nombre) >= 0) {
    return -1; /* "Ese molde ya existe" en este ambito */
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
  int posicion = tabla->cantidad++;
  Simbolo *simbolo = &tabla->simbolos[posicion];
  memset(simbolo, 0, sizeof(*simbolo));
  simbolo->nombre = copiar_texto(nombre);
  simbolo->tipo = tipo;
  simbolo->categoria = categoria;
  simbolo->ambito = copiar_texto(ambito_texto(tabla));
  simbolo->nivel = tabla->nivel_actual;
  simbolo->activo = 1;
  simbolo->linea_declaracion = linea;
  simbolo->tiene_valor_inicial = tiene_valor_inicial;
  simbolo->valor = valor_por_defecto(tipo);
  return posicion;
}

const char *categoria_nombre(Categoria categoria) {
  switch (categoria) {
  case CAT_MOLDE: return "molde";
  case CAT_SECRETO: return "secreto";
  case CAT_INGREDIENTE: return "ingrediente";
  case CAT_PREPARACION: return "preparación";
  }
  return "?";
}

/* ------------------------------------------------------------------ */
/* Impresion                                                           */
/* ------------------------------------------------------------------ */

void tabla_imprimir(const TablaSimbolos *tabla, int con_valores) {
  enum { COLUMNAS = 10 };
  const int anchos[COLUMNAS] = {20, 11, 9, 6, 20, 5, 14, 4, 7, 20};
  const char *encabezado[COLUMNAS] = {"Nombre", "Categoría", "Tipo",
                                      "En C",   "Ámbito",    "Línea",
                                      "Detalle", "Usos",     "Cambios",
                                      "Valor final"};
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
    char linea[16], usos[16], cambios[16], detalle[48], final[128];
    snprintf(linea, sizeof(linea), "%d", s->linea_declaracion);
    snprintf(usos, sizeof(usos), "%d", s->usos);
    snprintf(cambios, sizeof(cambios), "%d", s->modificaciones);

    switch (s->categoria) {
    case CAT_PREPARACION:
      snprintf(detalle, sizeof(detalle), "%d ingrediente%s", s->num_ingredientes,
               s->num_ingredientes == 1 ? "" : "s");
      snprintf(final, sizeof(final), "-");
      break;
    case CAT_INGREDIENTE:
      snprintf(detalle, sizeof(detalle), "parámetro");
      valor_a_texto(&s->valor, final, sizeof(final));
      break;
    default:
      snprintf(detalle, sizeof(detalle), "%s",
               s->tiene_valor_inicial ? "con valor" : "por defecto");
      valor_a_texto(&s->valor, final, sizeof(final));
      break;
    }

    const char *celdas[COLUMNAS] = {s->nombre,
                                    categoria_nombre(s->categoria),
                                    tipo_nombre(s->tipo),
                                    tipo_equivalente_c(s->tipo),
                                    s->ambito,
                                    linea,
                                    detalle,
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
    free(tabla->simbolos[i].ambito);
    valor_liberar(&tabla->simbolos[i].valor);
  }
  free(tabla->simbolos);
  free(tabla->ambito_actual);
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
  case TIPO_NADA:
    snprintf(destino, (size_t)tamano, "-");
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
