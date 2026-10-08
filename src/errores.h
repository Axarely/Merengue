/*
 * errores.h - Reporte de errores de cada fase del compilador.
 *
 * Los errores lexicos, sintacticos y semanticos se GUARDAN y luego se
 * muestran dentro de la seccion de su fase. Los errores de ejecucion
 * se muestran de inmediato.
 */
#ifndef ERRORES_H
#define ERRORES_H

typedef enum {
  FASE_LEXICA,
  FASE_SINTACTICA,
  FASE_SEMANTICA,
  FASE_EJECUCION
} Fase;

void reportar_error(Fase fase, int linea, const char *formato, ...);

/* Las advertencias no detienen la compilacion (se muestran con la fase semantica). */
void reportar_advertencia(int linea, const char *formato, ...);

int errores_en(Fase fase);
int total_advertencias(void);

/* Muestra (en stderr) los errores guardados de una fase. */
void errores_imprimir(Fase fase);
void advertencias_imprimir(void);

void errores_liberar(void);

#endif
