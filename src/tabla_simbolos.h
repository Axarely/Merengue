/*
 * tabla_simbolos.h - Tabla de Simbolos de Merengue ("la mesa de moldes").
 *
 * Cada nombre declarado ocupa un renglon: moldes, moldes secretos,
 * ingredientes (parametros) y preparaciones (funciones). Cada renglon
 * dice en que AMBITO vive:
 *   - "global": declarado en la receta principal,
 *   - el nombre de una preparacion: declarado dentro de ella,
 *   - "si L12" / "sino L14": declarado dentro de un bloque de un si.
 *
 * Los ambitos se apilan (una preparacion puede tener un si, y ese si otro
 * si). Al salir de un ambito sus nombres quedan INACTIVOS: ya no se pueden
 * usar, pero siguen en la tabla para mostrarlos al final.
 *
 * La llena el analizador semantico y la usa el interprete para guardar
 * el valor de cada molde mientras corre la receta.
 */
#ifndef TABLA_SIMBOLOS_H
#define TABLA_SIMBOLOS_H

#include "tipos.h"

typedef struct Valor {
  TipoDato tipo;           /* en una bandeja: el tipo de sus elementos */
  int porciones;
  double tazas;
  char *betun; /* memoria propia */
  char chispa;
  int punto;

  /* Bandejas (Regla 6): lista de valores del mismo tipo, como un array. */
  int es_bandeja;
  struct Valor *elementos; /* memoria propia */
  int largo;
  int capacidad;
} Valor;

typedef enum {
  CAT_MOLDE,       /* variable                        */
  CAT_SECRETO,     /* constante                       */
  CAT_INGREDIENTE, /* parametro de una preparacion    */
  CAT_PREPARACION  /* funcion                         */
} Categoria;

typedef struct {
  char *nombre;
  TipoDato tipo;           /* en una preparacion: el tipo que rinde */
  Categoria categoria;
  char *ambito;            /* "global" o el nombre de la preparacion */
  int nivel;               /* 0 = global, 1, 2, ... = ambitos anidados */
  int activo;              /* 0 cuando ya se salio de su ambito */
  int linea_declaracion;
  int tiene_valor_inicial; /* 1 si se declaro con 'con' */
  int usos;                /* lecturas (o llamadas, si es preparacion) */
  int modificaciones;      /* veces que recibe vierte/espolvorea/raspa */
  int num_ingredientes;    /* solo preparaciones */
  const void *definicion;  /* solo preparaciones: su nodo del AST */
  int primer_local;        /* solo preparaciones: posicion de su primer */
  int num_locales;         /* ingrediente/molde local y cuantos tiene  */
  int es_bandeja;          /* 1 si es una bandeja (array) */
  Valor valor;             /* valor durante la ejecucion */
} Simbolo;

typedef struct {
  Simbolo *simbolos;
  int cantidad;
  int capacidad;
  int nivel_actual;
  char *ambitos[64]; /* nombre de cada ambito abierto (0 = global) */
} TablaSimbolos;

void tabla_iniciar(TablaSimbolos *tabla);

/* Abre un ambito nuevo (preparacion o bloque de un si) / lo cierra. */
void tabla_entrar_ambito(TablaSimbolos *tabla, const char *nombre);
void tabla_salir_ambito(TablaSimbolos *tabla);

/* Agrega un nombre al ambito actual. Devuelve su posicion, o -1 si ya
   existia en ese mismo ambito. */
int tabla_insertar(TablaSimbolos *tabla, const char *nombre, TipoDato tipo,
                   Categoria categoria, int linea, int tiene_valor_inicial);

/* Busca un nombre visible: primero en el ambito actual y luego en los
   de afuera, hasta el global. Devuelve su posicion o -1. */
int tabla_buscar(const TablaSimbolos *tabla, const char *nombre);

/* Busca solo en el ambito global. */
int tabla_buscar_global(const TablaSimbolos *tabla, const char *nombre);

/* Busca solo en el ambito actual. */
int tabla_buscar_local(const TablaSimbolos *tabla, const char *nombre);

Simbolo *tabla_simbolo(TablaSimbolos *tabla, int posicion);

const char *categoria_nombre(Categoria categoria);

/* Imprime la tabla. Si con_valores es 1 agrega la columna "Valor final". */
void tabla_imprimir(const TablaSimbolos *tabla, int con_valores);

void tabla_liberar(TablaSimbolos *tabla);

/* Valor por defecto de cada tipo: 0, 0,0, "", ' ', crudo. */
Valor valor_por_defecto(TipoDato tipo);

/* Texto del valor, al estilo Merengue (2,5  "hola"  'A'  dorado). */
void valor_a_texto(const Valor *valor, char *destino, int tamano);

/* Bandeja vacia cuyos elementos son del tipo dado. */
Valor valor_bandeja_vacia(TipoDato tipo);

/* Agrega una copia de "elemento" al final de la bandeja. */
void bandeja_agregar(Valor *bandeja, const Valor *elemento);

/* Reemplaza el valor guardado (copia el betun y los elementos). */
void valor_asignar(Valor *destino, const Valor *origen);

void valor_liberar(Valor *valor);

#endif
