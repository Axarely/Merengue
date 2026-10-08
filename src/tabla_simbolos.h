/*
 * tabla_simbolos.h - Tabla de Simbolos de Merengue ("la mesa de moldes").
 *
 * Cada molde declarado ocupa un renglon con su nombre, tipo, si es
 * secreto (constante), la linea donde se declaro y cuantas veces se usa.
 * La llena el analizador semantico y la usa el interprete para guardar
 * el valor de cada molde mientras corre la receta.
 */
#ifndef TABLA_SIMBOLOS_H
#define TABLA_SIMBOLOS_H

#include "tipos.h"

typedef struct {
  TipoDato tipo;
  int porciones;
  double tazas;
  char *betun; /* memoria propia */
  char chispa;
  int punto;
} Valor;

typedef struct {
  char *nombre;
  TipoDato tipo;
  int es_secreto;
  int linea_declaracion;
  int tiene_valor_inicial; /* 1 si se declaro con 'con' */
  int usos;                /* veces que se lee dentro de una expresion */
  int modificaciones;      /* veces que recibe vierte/espolvorea/raspa */
  Valor valor;             /* valor durante la ejecucion */
} Simbolo;

typedef struct {
  Simbolo *simbolos;
  int cantidad;
  int capacidad;
} TablaSimbolos;

void tabla_iniciar(TablaSimbolos *tabla);

/* Devuelve el simbolo nuevo, o NULL si el nombre ya existia. */
Simbolo *tabla_insertar(TablaSimbolos *tabla, const char *nombre, TipoDato tipo,
                        int es_secreto, int linea, int tiene_valor_inicial);

Simbolo *tabla_buscar(TablaSimbolos *tabla, const char *nombre);

/* Imprime la tabla. Si con_valores es 1 agrega la columna "Valor final". */
void tabla_imprimir(const TablaSimbolos *tabla, int con_valores);

void tabla_liberar(TablaSimbolos *tabla);

/* Valor por defecto de cada tipo: 0, 0,0, "", ' ', crudo. */
Valor valor_por_defecto(TipoDato tipo);

/* Texto del valor, al estilo Merengue (2,5  "hola"  'A'  dorado). */
void valor_a_texto(const Valor *valor, char *destino, int tamano);

/* Reemplaza el valor guardado (copia el betun). */
void valor_asignar(Valor *destino, const Valor *origen);

void valor_liberar(Valor *valor);

#endif
