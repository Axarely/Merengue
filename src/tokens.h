/*
 * tokens.h - Registro de los tokens que encuentra el analizador lexico.
 *
 * El lexer (lexico.l) anota cada token aqui antes de entregarlo al
 * parser, para poder mostrar la tabla de tokens en la presentacion.
 */
#ifndef TOKENS_H
#define TOKENS_H

void tokens_registrar(int linea, const char *categoria, const char *lexema);
void tokens_imprimir(void);
int tokens_total(void);
void tokens_liberar(void);

/* Convierte la secuencia de escape de una chispa ('\n', '\'') en un char. */
char chispa_desde_texto(const char *texto);

/* Quita las comillas de un betun y resuelve \n, \t, \" y \\. */
char *betun_desde_texto(const char *texto);

#endif
