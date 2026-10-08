/*
 * formato.h - Ayudas para imprimir tablas y valores de Merengue.
 */
#ifndef FORMATO_H
#define FORMATO_H

/* Imprime "texto" ocupando "ancho" columnas (cuenta tildes como 1 columna). */
void imprimir_celda(const char *texto, int ancho);

/* Imprime una linea separadora: +-----+-------+ segun los anchos. */
void imprimir_separador(const int *anchos, int columnas);

/* Imprime una fila completa de una tabla. */
void imprimir_fila(const char **celdas, const int *anchos, int columnas);

/* Escribe un decimal al estilo Merengue: 2,5 en lugar de 2.5 */
void formatear_tazas(double valor, char *destino, int tamano);

/* Imprime un titulo de seccion. */
void imprimir_titulo(const char *titulo);

#endif
