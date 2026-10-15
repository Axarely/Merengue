/*
 * tipos.h - Los cinco tipos de dato de Merengue.
 *
 *   porciones -> enteros            (como int en C)
 *   tazas     -> decimales          (como float en C)
 *   betun     -> texto              (como char[] en C)
 *   chispa    -> un solo caracter   (como char en C)
 *   punto     -> dorado / crudo     (verdadero / falso)
 *
 * Ademas existe "nada" (como void en C), que solo se usa para decir que
 * una preparacion no entrega ningun valor: rinde nada.
 */
#ifndef TIPOS_H
#define TIPOS_H

typedef enum {
  TIPO_PORCIONES,
  TIPO_TAZAS,
  TIPO_BETUN,
  TIPO_CHISPA,
  TIPO_PUNTO,
  TIPO_NADA,       /* solo para preparaciones: rinde nada (void) */
  TIPO_DESCONOCIDO /* se usa cuando una expresion tiene un error */
} TipoDato;

/* Nombre del tipo tal como se escribe en Merengue ("porciones", "tazas"...). */
const char *tipo_nombre(TipoDato tipo);

/* Equivalente en C, para la tabla de simbolos. */
const char *tipo_equivalente_c(TipoDato tipo);

/* Convierte la palabra del codigo fuente en un TipoDato. */
TipoDato tipo_desde_texto(const char *texto);

/* 1 si el tipo es numerico (porciones o tazas). */
int tipo_es_numerico(TipoDato tipo);

/* Copia de una cadena en memoria nueva (termina el programa si no hay memoria). */
char *copiar_texto(const char *texto);

#endif
