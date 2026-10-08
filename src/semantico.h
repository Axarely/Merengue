/*
 * semantico.h - Analizador SEMANTICO de Merengue.
 *
 * Recorre el AST instruccion por instruccion (en el orden de la receta)
 * y revisa el SIGNIFICADO del programa:
 *   - que cada molde se declare antes de usarse,
 *   - que no se declare dos veces el mismo nombre,
 *   - que un molde secreto tenga valor y nunca cambie,
 *   - que los tipos combinen (si no: "La mezcla se corto"),
 *   - que espolvorea/raspa se usen con tipos que lo permiten.
 *
 * Mientras revisa, llena la Tabla de Simbolos y anota en cada nodo del
 * AST el tipo de dato de su expresion.
 */
#ifndef SEMANTICO_H
#define SEMANTICO_H

#include "ast.h"
#include "tabla_simbolos.h"

/* Devuelve la cantidad de errores semanticos encontrados (0 = todo bien). */
int analizar_semantica(Nodo *receta, TablaSimbolos *tabla);

#endif
