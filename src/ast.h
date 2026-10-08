/*
 * ast.h - Arbol de Sintaxis Abstracta (AST) de Merengue.
 *
 * El analizador sintactico (sintactico.y) crea un nodo cada vez que
 * reconoce una regla. Cada nodo guarda:
 *   - un numero unico (id) para la tabla AST,
 *   - la clase de construccion (declaracion, asignacion, literal...),
 *   - la linea del codigo fuente (para los mensajes de error),
 *   - sus hijos, en orden.
 *
 * El tipo de dato de cada expresion (tipo_dato) lo llena despues el
 * analizador semantico.
 */
#ifndef AST_H
#define AST_H

#include "tipos.h"

typedef enum {
  NODO_RECETA,        /* raiz: el programa completo                */
  NODO_DECLARACION,   /* molde [secreto] <tipo> <nombre> [con expr] */
  NODO_ASIGNACION,    /* vierte / espolvorea / raspa                */
  NODO_SIRVE,         /* sirve expr.  (mostrar en pantalla)          */
  NODO_LITERAL,       /* 3   2,5   "texto"   'A'   dorado            */
  NODO_IDENTIFICADOR, /* uso de un molde dentro de una expresion     */
  NODO_OPERACION,     /* expr + expr, expr - expr, expr * expr ...   */
  NODO_NEGATIVO,      /* -expr                                       */
  NODO_FUNCION        /* degustar(expr)  rebanar(expr)               */
} TipoNodo;

typedef enum { ASIG_VIERTE, ASIG_ESPOLVOREA, ASIG_RASPA } TipoAsignacion;

typedef enum { FUNC_DEGUSTAR, FUNC_REBANAR } TipoFuncion;

typedef struct Nodo {
  int id;
  TipoNodo clase;
  int linea;

  /* Tipo de la expresion. En literales lo pone el parser; en el resto
     lo calcula el analizador semantico. */
  TipoDato tipo_dato;

  /* Nombre del molde (declaracion, asignacion, identificador). */
  char *nombre;

  /* Datos propios de cada clase de nodo. */
  TipoDato tipo_declarado; /* NODO_DECLARACION */
  int es_secreto;          /* NODO_DECLARACION */
  TipoAsignacion asignacion;
  char operador;           /* '+', '-', '*', '/' en NODO_OPERACION */
  TipoFuncion funcion;

  /* Valor de un NODO_LITERAL. */
  union {
    int porciones;
    double tazas;
    char *betun;
    char chispa;
    int punto;
  } valor;

  /* Hijos en orden (lista dinamica). */
  struct Nodo **hijos;
  int num_hijos;
  int capacidad;
} Nodo;

/* Constructores (los llama el parser). */
Nodo *nodo_receta(void);
Nodo *nodo_declaracion(int linea, int es_secreto, TipoDato tipo,
                       const char *nombre, Nodo *valor_inicial);
Nodo *nodo_asignacion(int linea, TipoAsignacion clase, Nodo *expresion,
                      const char *nombre);
Nodo *nodo_sirve(int linea, Nodo *expresion);
Nodo *nodo_literal_porciones(int linea, int valor);
Nodo *nodo_literal_tazas(int linea, double valor);
Nodo *nodo_literal_betun(int linea, const char *valor);
Nodo *nodo_literal_chispa(int linea, char valor);
Nodo *nodo_literal_punto(int linea, int valor);
Nodo *nodo_identificador(int linea, const char *nombre);
Nodo *nodo_operacion(int linea, char operador, Nodo *izquierda, Nodo *derecha);
Nodo *nodo_negativo(int linea, Nodo *expresion);
Nodo *nodo_funcion(int linea, TipoFuncion funcion, Nodo *argumento);

void nodo_agregar_hijo(Nodo *padre, Nodo *hijo);

/* Numera los nodos en preorden (1, 2, 3...) para la tabla AST. */
void ast_numerar(Nodo *raiz);

/* Dibuja el arbol con ramas (├── └──). */
void ast_imprimir_arbol(const Nodo *raiz);

/* Imprime la tabla AST: un renglon por nodo con su padre e hijos. */
void ast_imprimir_tabla(const Nodo *raiz);

/* Texto corto que describe el nodo, p. ej. "Operacion(*)". */
void ast_describir(const Nodo *nodo, char *destino, int tamano);

const char *ast_nombre_clase(TipoNodo clase);

void ast_liberar(Nodo *nodo);

#endif
