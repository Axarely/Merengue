/*
 * interprete.h - Ejecuta ("hornea") la receta recorriendo el AST.
 *
 * Solo se llama si no hubo errores lexicos, sintacticos ni semanticos.
 * Guarda el valor de cada molde en la Tabla de Simbolos.
 */
#ifndef INTERPRETE_H
#define INTERPRETE_H

#include "ast.h"
#include "tabla_simbolos.h"

/* Devuelve 1 si la receta termino bien, 0 si hubo un error de ejecucion. */
int hornear_receta(const Nodo *receta, TablaSimbolos *tabla);

#endif
