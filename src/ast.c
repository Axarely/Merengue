#include "ast.h"
#include "formato.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Creacion de nodos                                                   */
/* ------------------------------------------------------------------ */

static Nodo *nodo_nuevo(TipoNodo clase, int linea) {
  Nodo *nodo = calloc(1, sizeof(*nodo));
  if (nodo == NULL) {
    fprintf(stderr, "Sin memoria\n");
    exit(EXIT_FAILURE);
  }
  nodo->clase = clase;
  nodo->linea = linea;
  nodo->tipo_dato = TIPO_DESCONOCIDO;
  nodo->simbolo = -1;
  return nodo;
}

void nodo_agregar_hijo(Nodo *padre, Nodo *hijo) {
  if (hijo == NULL) {
    return;
  }
  if (padre->num_hijos == padre->capacidad) {
    int nueva = padre->capacidad == 0 ? 4 : padre->capacidad * 2;
    Nodo **hijos = realloc(padre->hijos, (size_t)nueva * sizeof(*hijos));
    if (hijos == NULL) {
      fprintf(stderr, "Sin memoria\n");
      exit(EXIT_FAILURE);
    }
    padre->hijos = hijos;
    padre->capacidad = nueva;
  }
  padre->hijos[padre->num_hijos++] = hijo;
}

Nodo *nodo_receta(void) { return nodo_nuevo(NODO_RECETA, 1); }

Nodo *nodo_declaracion(int linea, int es_secreto, TipoDato tipo,
                       const char *nombre, Nodo *valor_inicial) {
  Nodo *nodo = nodo_nuevo(NODO_DECLARACION, linea);
  nodo->es_secreto = es_secreto;
  nodo->tipo_declarado = tipo;
  nodo->tipo_dato = tipo;
  nodo->nombre = copiar_texto(nombre);
  nodo_agregar_hijo(nodo, valor_inicial);
  return nodo;
}

Nodo *nodo_asignacion(int linea, TipoAsignacion clase, Nodo *expresion,
                      const char *nombre) {
  Nodo *nodo = nodo_nuevo(NODO_ASIGNACION, linea);
  nodo->asignacion = clase;
  nodo->nombre = copiar_texto(nombre);
  nodo_agregar_hijo(nodo, expresion);
  return nodo;
}

Nodo *nodo_sirve(int linea, Nodo *expresion) {
  Nodo *nodo = nodo_nuevo(NODO_SIRVE, linea);
  nodo_agregar_hijo(nodo, expresion);
  return nodo;
}

Nodo *nodo_literal_porciones(int linea, int valor) {
  Nodo *nodo = nodo_nuevo(NODO_LITERAL, linea);
  nodo->tipo_dato = TIPO_PORCIONES;
  nodo->valor.porciones = valor;
  return nodo;
}

Nodo *nodo_literal_tazas(int linea, double valor) {
  Nodo *nodo = nodo_nuevo(NODO_LITERAL, linea);
  nodo->tipo_dato = TIPO_TAZAS;
  nodo->valor.tazas = valor;
  return nodo;
}

Nodo *nodo_literal_betun(int linea, const char *valor) {
  Nodo *nodo = nodo_nuevo(NODO_LITERAL, linea);
  nodo->tipo_dato = TIPO_BETUN;
  nodo->valor.betun = copiar_texto(valor);
  return nodo;
}

Nodo *nodo_literal_chispa(int linea, char valor) {
  Nodo *nodo = nodo_nuevo(NODO_LITERAL, linea);
  nodo->tipo_dato = TIPO_CHISPA;
  nodo->valor.chispa = valor;
  return nodo;
}

Nodo *nodo_literal_punto(int linea, int valor) {
  Nodo *nodo = nodo_nuevo(NODO_LITERAL, linea);
  nodo->tipo_dato = TIPO_PUNTO;
  nodo->valor.punto = valor;
  return nodo;
}

Nodo *nodo_identificador(int linea, const char *nombre) {
  Nodo *nodo = nodo_nuevo(NODO_IDENTIFICADOR, linea);
  nodo->nombre = copiar_texto(nombre);
  return nodo;
}

Nodo *nodo_operacion(int linea, char operador, Nodo *izquierda, Nodo *derecha) {
  Nodo *nodo = nodo_nuevo(NODO_OPERACION, linea);
  nodo->operador = operador;
  nodo_agregar_hijo(nodo, izquierda);
  nodo_agregar_hijo(nodo, derecha);
  return nodo;
}

Nodo *nodo_negativo(int linea, Nodo *expresion) {
  Nodo *nodo = nodo_nuevo(NODO_NEGATIVO, linea);
  nodo_agregar_hijo(nodo, expresion);
  return nodo;
}

Nodo *nodo_funcion(int linea, TipoFuncion funcion, Nodo *argumento) {
  Nodo *nodo = nodo_nuevo(NODO_FUNCION, linea);
  nodo->funcion = funcion;
  nodo_agregar_hijo(nodo, argumento);
  return nodo;
}

Nodo *nodo_preparacion(int linea, const char *nombre, Nodo *ingredientes,
                       TipoDato rinde, Nodo *pasos) {
  Nodo *nodo = nodo_nuevo(NODO_PREPARACION, linea);
  nodo->nombre = copiar_texto(nombre);
  nodo->tipo_declarado = rinde;
  nodo->tipo_dato = rinde;
  nodo_agregar_hijo(nodo, ingredientes);
  nodo_agregar_hijo(nodo, pasos);
  return nodo;
}

Nodo *nodo_ingredientes(int linea) { return nodo_nuevo(NODO_INGREDIENTES, linea); }

Nodo *nodo_ingrediente(int linea, TipoDato tipo, const char *nombre) {
  Nodo *nodo = nodo_nuevo(NODO_INGREDIENTE, linea);
  nodo->tipo_declarado = tipo;
  nodo->tipo_dato = tipo;
  nodo->nombre = copiar_texto(nombre);
  return nodo;
}

Nodo *nodo_pasos(int linea) { return nodo_nuevo(NODO_PASOS, linea); }

Nodo *nodo_entrega(int linea, Nodo *valor) {
  Nodo *nodo = nodo_nuevo(NODO_ENTREGA, linea);
  nodo_agregar_hijo(nodo, valor);
  return nodo;
}

Nodo *nodo_lista_temporal(void) { return nodo_nuevo(NODO_PASOS, 0); }

Nodo *nodo_llamada(int linea, const char *nombre, Nodo *argumentos) {
  Nodo *nodo = nodo_nuevo(NODO_LLAMADA, linea);
  nodo->nombre = copiar_texto(nombre);
  if (argumentos != NULL) {
    /* Los argumentos pasan a ser hijos directos de la llamada. */
    nodo->hijos = argumentos->hijos;
    nodo->num_hijos = argumentos->num_hijos;
    nodo->capacidad = argumentos->capacidad;
    argumentos->hijos = NULL;
    argumentos->num_hijos = argumentos->capacidad = 0;
    ast_liberar(argumentos);
  }
  return nodo;
}

Nodo *nodo_si(int linea, Nodo *condicion, Nodo *entonces, Nodo *sino) {
  Nodo *nodo = nodo_nuevo(NODO_SI, linea);
  nodo_agregar_hijo(nodo, condicion);
  nodo_agregar_hijo(nodo, entonces);
  nodo_agregar_hijo(nodo, sino);
  return nodo;
}

Nodo *nodo_bloque(TipoNodo clase, Nodo *pasos) {
  pasos->clase = clase;
  return pasos;
}

Nodo *nodo_comparacion(int linea, TipoComparacion comparacion, Nodo *izquierda,
                       Nodo *derecha) {
  Nodo *nodo = nodo_nuevo(NODO_COMPARACION, linea);
  nodo->comparacion = comparacion;
  nodo_agregar_hijo(nodo, izquierda);
  nodo_agregar_hijo(nodo, derecha);
  return nodo;
}

Nodo *nodo_logica(int linea, TipoLogica logica, Nodo *izquierda, Nodo *derecha) {
  Nodo *nodo = nodo_nuevo(NODO_LOGICA, linea);
  nodo->logica = logica;
  nodo_agregar_hijo(nodo, izquierda);
  nodo_agregar_hijo(nodo, derecha);
  return nodo;
}

Nodo *nodo_no(int linea, Nodo *expresion) {
  Nodo *nodo = nodo_nuevo(NODO_NO, linea);
  nodo_agregar_hijo(nodo, expresion);
  return nodo;
}

const char *comparacion_texto(TipoComparacion comparacion) {
  switch (comparacion) {
  case COMP_MAYOR: return "mayor que";
  case COMP_MENOR: return "menor que";
  case COMP_MAYOR_IGUAL: return "mayor o igual que";
  case COMP_MENOR_IGUAL: return "menor o igual que";
  case COMP_IGUAL: return "igual a";
  case COMP_DISTINTO: return "distinto de";
  }
  return "?";
}

/* ------------------------------------------------------------------ */
/* Descripcion de nodos                                                */
/* ------------------------------------------------------------------ */

const char *ast_nombre_clase(TipoNodo clase) {
  switch (clase) {
  case NODO_RECETA: return "Receta";
  case NODO_DECLARACION: return "Declaración";
  case NODO_ASIGNACION: return "Asignación";
  case NODO_SIRVE: return "Sirve";
  case NODO_LITERAL: return "Literal";
  case NODO_IDENTIFICADOR: return "Identificador";
  case NODO_OPERACION: return "Operación";
  case NODO_NEGATIVO: return "Negativo";
  case NODO_FUNCION: return "Función";
  case NODO_PREPARACION: return "Preparación";
  case NODO_INGREDIENTES: return "Ingredientes";
  case NODO_INGREDIENTE: return "Ingrediente";
  case NODO_PASOS: return "Pasos";
  case NODO_ENTREGA: return "Entrega";
  case NODO_LLAMADA: return "Llamada";
  case NODO_SI: return "Si";
  case NODO_ENTONCES: return "Entonces";
  case NODO_SINO: return "Sino";
  case NODO_COMPARACION: return "Comparación";
  case NODO_LOGICA: return "Lógica";
  case NODO_NO: return "No";
  }
  return "?";
}

static const char *nombre_asignacion(TipoAsignacion clase) {
  switch (clase) {
  case ASIG_VIERTE: return "vierte";
  case ASIG_ESPOLVOREA: return "espolvorea";
  case ASIG_RASPA: return "raspa";
  }
  return "?";
}

static void describir_literal(const Nodo *nodo, char *destino, int tamano) {
  char numero[64];
  switch (nodo->tipo_dato) {
  case TIPO_PORCIONES:
    snprintf(destino, (size_t)tamano, "%d", nodo->valor.porciones);
    break;
  case TIPO_TAZAS:
    formatear_tazas(nodo->valor.tazas, numero, sizeof(numero));
    snprintf(destino, (size_t)tamano, "%s", numero);
    break;
  case TIPO_BETUN:
    snprintf(destino, (size_t)tamano, "\"%s\"", nodo->valor.betun);
    break;
  case TIPO_CHISPA:
    if (nodo->valor.chispa == '\n') {
      snprintf(destino, (size_t)tamano, "'\\n'");
    } else {
      snprintf(destino, (size_t)tamano, "'%c'", nodo->valor.chispa);
    }
    break;
  case TIPO_PUNTO:
    snprintf(destino, (size_t)tamano, "%s", nodo->valor.punto ? "dorado" : "crudo");
    break;
  case TIPO_NADA:
  case TIPO_DESCONOCIDO:
    snprintf(destino, (size_t)tamano, "?");
    break;
  }
}

/* Detalle sin el nombre de la clase: "molde secreto porciones huevos". */
static void describir_detalle(const Nodo *nodo, char *destino, int tamano) {
  switch (nodo->clase) {
  case NODO_RECETA:
    snprintf(destino, (size_t)tamano, "%d instrucci%s", nodo->num_hijos,
             nodo->num_hijos == 1 ? "ón" : "ones");
    break;
  case NODO_DECLARACION:
    snprintf(destino, (size_t)tamano, "molde %s%s %s",
             nodo->es_secreto ? "secreto " : "",
             tipo_nombre(nodo->tipo_declarado), nodo->nombre);
    break;
  case NODO_ASIGNACION:
    snprintf(destino, (size_t)tamano, "%s -> %s",
             nombre_asignacion(nodo->asignacion), nodo->nombre);
    break;
  case NODO_SIRVE:
    snprintf(destino, (size_t)tamano, "mostrar en pantalla");
    break;
  case NODO_LITERAL:
    describir_literal(nodo, destino, tamano);
    break;
  case NODO_IDENTIFICADOR:
    snprintf(destino, (size_t)tamano, "%s", nodo->nombre);
    break;
  case NODO_OPERACION:
    snprintf(destino, (size_t)tamano, "%c", nodo->operador);
    break;
  case NODO_NEGATIVO:
    snprintf(destino, (size_t)tamano, "-");
    break;
  case NODO_FUNCION:
    snprintf(destino, (size_t)tamano, "%s()",
             nodo->funcion == FUNC_DEGUSTAR ? "degustar" : "rebanar");
    break;
  case NODO_PREPARACION:
    snprintf(destino, (size_t)tamano, "%s rinde %s", nodo->nombre,
             tipo_nombre(nodo->tipo_declarado));
    break;
  case NODO_INGREDIENTES:
    snprintf(destino, (size_t)tamano, "%d", nodo->num_hijos);
    break;
  case NODO_INGREDIENTE:
    snprintf(destino, (size_t)tamano, "%s %s", tipo_nombre(nodo->tipo_declarado),
             nodo->nombre);
    break;
  case NODO_PASOS:
    snprintf(destino, (size_t)tamano, "%d instrucci%s", nodo->num_hijos,
             nodo->num_hijos == 1 ? "ón" : "ones");
    break;
  case NODO_ENTREGA:
    snprintf(destino, (size_t)tamano, "%s",
             nodo->num_hijos > 0 ? "devuelve un valor" : "sin valor");
    break;
  case NODO_LLAMADA:
    snprintf(destino, (size_t)tamano, "%s, %d argumento%s", nodo->nombre,
             nodo->num_hijos, nodo->num_hijos == 1 ? "" : "s");
    break;
  case NODO_SI:
    snprintf(destino, (size_t)tamano, "%s",
             nodo->num_hijos > 2 ? "con sino" : "sin sino");
    break;
  case NODO_ENTONCES:
  case NODO_SINO:
    snprintf(destino, (size_t)tamano, "%d instrucci%s", nodo->num_hijos,
             nodo->num_hijos == 1 ? "ón" : "ones");
    break;
  case NODO_COMPARACION:
    snprintf(destino, (size_t)tamano, "%s", comparacion_texto(nodo->comparacion));
    break;
  case NODO_LOGICA:
    snprintf(destino, (size_t)tamano, "%s",
             nodo->logica == LOGICA_O ? "o" : "y ademas");
    break;
  case NODO_NO:
    snprintf(destino, (size_t)tamano, "no");
    break;
  }
}

void ast_describir(const Nodo *nodo, char *destino, int tamano) {
  char detalle[160];
  describir_detalle(nodo, detalle, sizeof(detalle));
  snprintf(destino, (size_t)tamano, "%s(%s)", ast_nombre_clase(nodo->clase),
           detalle);
}

static int es_expresion(const Nodo *nodo) {
  return nodo->clase == NODO_LITERAL || nodo->clase == NODO_IDENTIFICADOR ||
         nodo->clase == NODO_OPERACION || nodo->clase == NODO_NEGATIVO ||
         nodo->clase == NODO_FUNCION || nodo->clase == NODO_LLAMADA ||
         nodo->clase == NODO_COMPARACION || nodo->clase == NODO_LOGICA ||
         nodo->clase == NODO_NO;
}

/* ------------------------------------------------------------------ */
/* Numeracion en preorden                                              */
/* ------------------------------------------------------------------ */

static void numerar(Nodo *nodo, int *siguiente) {
  nodo->id = (*siguiente)++;
  for (int i = 0; i < nodo->num_hijos; i++) {
    numerar(nodo->hijos[i], siguiente);
  }
}

void ast_numerar(Nodo *raiz) {
  int siguiente = 1;
  if (raiz != NULL) {
    numerar(raiz, &siguiente);
  }
}

/* ------------------------------------------------------------------ */
/* Arbol con ramas                                                     */
/* ------------------------------------------------------------------ */

static void imprimir_rama(const Nodo *nodo, const char *prefijo, int es_ultimo,
                          int es_raiz) {
  char descripcion[220];
  ast_describir(nodo, descripcion, sizeof(descripcion));

  if (es_raiz) {
    printf("%s", descripcion);
  } else {
    printf("%s%s%s", prefijo, es_ultimo ? "└── " : "├── ", descripcion);
  }
  if (es_expresion(nodo) && nodo->tipo_dato != TIPO_DESCONOCIDO) {
    printf("  : %s", tipo_nombre(nodo->tipo_dato));
  }
  if (nodo->clase == NODO_DECLARACION || nodo->clase == NODO_ASIGNACION ||
      nodo->clase == NODO_SIRVE || nodo->clase == NODO_PREPARACION ||
      nodo->clase == NODO_ENTREGA || nodo->clase == NODO_SI) {
    printf("   [línea %d]", nodo->linea);
  }
  printf("\n");

  char nuevo_prefijo[512];
  if (es_raiz) {
    nuevo_prefijo[0] = '\0';
  } else {
    snprintf(nuevo_prefijo, sizeof(nuevo_prefijo), "%s%s", prefijo,
             es_ultimo ? "    " : "│   ");
  }
  for (int i = 0; i < nodo->num_hijos; i++) {
    imprimir_rama(nodo->hijos[i], nuevo_prefijo, i == nodo->num_hijos - 1, 0);
  }
}

void ast_imprimir_arbol(const Nodo *raiz) {
  if (raiz == NULL) {
    printf("(arbol vacio)\n");
    return;
  }
  imprimir_rama(raiz, "", 1, 1);
}

/* ------------------------------------------------------------------ */
/* Tabla AST                                                           */
/* ------------------------------------------------------------------ */

enum { COLUMNAS_AST = 7 };
static const int anchos_ast[COLUMNAS_AST] = {4, 13, 38, 10, 5, 5, 18};

static void fila_ast(const Nodo *nodo, int id_padre) {
  char id[16], detalle[160], tipo[32], linea[16], padre[16], hijos[64];

  snprintf(id, sizeof(id), "%d", nodo->id);
  describir_detalle(nodo, detalle, sizeof(detalle));
  if (es_expresion(nodo) || nodo->clase == NODO_DECLARACION ||
      nodo->clase == NODO_INGREDIENTE || nodo->clase == NODO_PREPARACION) {
    snprintf(tipo, sizeof(tipo), "%s", tipo_nombre(nodo->tipo_dato));
  } else {
    snprintf(tipo, sizeof(tipo), "-");
  }
  snprintf(linea, sizeof(linea), "%d", nodo->linea);
  if (id_padre == 0) {
    snprintf(padre, sizeof(padre), "-");
  } else {
    snprintf(padre, sizeof(padre), "%d", id_padre);
  }

  hijos[0] = '\0';
  for (int i = 0; i < nodo->num_hijos; i++) {
    char numero[16];
    snprintf(numero, sizeof(numero), i == 0 ? "%d" : ", %d", nodo->hijos[i]->id);
    if (strlen(hijos) + strlen(numero) < sizeof(hijos) - 1) {
      strcat(hijos, numero);
    }
  }
  if (nodo->num_hijos == 0) {
    snprintf(hijos, sizeof(hijos), "- (hoja)");
  }

  const char *celdas[COLUMNAS_AST] = {
      id, ast_nombre_clase(nodo->clase), detalle, tipo, linea, padre, hijos};
  imprimir_fila(celdas, anchos_ast, COLUMNAS_AST);

  for (int i = 0; i < nodo->num_hijos; i++) {
    fila_ast(nodo->hijos[i], nodo->id);
  }
}

void ast_imprimir_tabla(const Nodo *raiz) {
  const char *encabezado[COLUMNAS_AST] = {"ID",    "Nodo",  "Detalle", "Tipo",
                                          "Línea", "Padre", "Hijos"};
  imprimir_separador(anchos_ast, COLUMNAS_AST);
  imprimir_fila(encabezado, anchos_ast, COLUMNAS_AST);
  imprimir_separador(anchos_ast, COLUMNAS_AST);
  if (raiz != NULL) {
    fila_ast(raiz, 0);
  }
  imprimir_separador(anchos_ast, COLUMNAS_AST);
}

/* ------------------------------------------------------------------ */
/* Liberar memoria                                                     */
/* ------------------------------------------------------------------ */

void ast_liberar(Nodo *nodo) {
  if (nodo == NULL) {
    return;
  }
  for (int i = 0; i < nodo->num_hijos; i++) {
    ast_liberar(nodo->hijos[i]);
  }
  free(nodo->hijos);
  free(nodo->nombre);
  if (nodo->clase == NODO_LITERAL && nodo->tipo_dato == TIPO_BETUN) {
    free(nodo->valor.betun);
  }
  free(nodo);
}
