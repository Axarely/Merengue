#include "interprete.h"
#include "errores.h"
#include "formato.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static TablaSimbolos *mesa;

/* Resultado de ejecutar una lista de instrucciones. */
typedef enum { EJ_ERROR = 0, EJ_OK = 1, EJ_ENTREGA = 2 } Resultado;

static Resultado ejecutar_instrucciones(Nodo *const *instrucciones, int cantidad,
                                        Valor *entregado);
static int llamar_preparacion(const Nodo *llamada, Valor *salida);

/* Limite de preparaciones abiertas a la vez (una dentro de otra). */
#define PROFUNDIDAD_MAXIMA 200
static int profundidad;

/* Convierte un valor al tipo del molde (unica conversion: porciones -> tazas). */
static Valor convertir(Valor valor, TipoDato destino) {
  if (destino == TIPO_TAZAS && valor.tipo == TIPO_PORCIONES) {
    valor.tazas = (double)valor.porciones;
    valor.tipo = TIPO_TAZAS;
  }
  return valor;
}

static double como_decimal(const Valor *valor) {
  return valor->tipo == TIPO_TAZAS ? valor->tazas : (double)valor->porciones;
}

/* Guarda un resultado entero revisando que quepa en porciones (int). */
static int resultado_porciones(long long resultado, int linea, Valor *salida) {
  if (resultado < INT_MIN || resultado > INT_MAX) {
    reportar_error(FASE_EJECUCION, linea,
                   "se desbordó el molde: el resultado %lld no cabe en porciones.",
                   resultado);
    return 0;
  }
  salida->tipo = TIPO_PORCIONES;
  salida->porciones = (int)resultado;
  return 1;
}

/* Evalua una expresion. Si el resultado es betun, quien llama lo libera. */
static int evaluar(const Nodo *nodo, Valor *salida) {
  memset(salida, 0, sizeof(*salida));

  switch (nodo->clase) {
  case NODO_LITERAL:
    salida->tipo = nodo->tipo_dato;
    switch (nodo->tipo_dato) {
    case TIPO_PORCIONES: salida->porciones = nodo->valor.porciones; break;
    case TIPO_TAZAS: salida->tazas = nodo->valor.tazas; break;
    case TIPO_BETUN: salida->betun = copiar_texto(nodo->valor.betun); break;
    case TIPO_CHISPA: salida->chispa = nodo->valor.chispa; break;
    case TIPO_PUNTO: salida->punto = nodo->valor.punto; break;
    case TIPO_NADA:
    case TIPO_DESCONOCIDO: return 0;
    }
    return 1;

  case NODO_IDENTIFICADOR: {
    Simbolo *simbolo = tabla_simbolo(mesa, nodo->simbolo);
    if (simbolo == NULL) return 0;
    *salida = simbolo->valor;
    if (salida->tipo == TIPO_BETUN) {
      salida->betun = copiar_texto(simbolo->valor.betun);
    }
    return 1;
  }

  case NODO_OPERACION: {
    Valor izquierda, derecha;
    if (!evaluar(nodo->hijos[0], &izquierda)) return 0;
    if (!evaluar(nodo->hijos[1], &derecha)) return 0;

    if (nodo->tipo_dato == TIPO_TAZAS) {
      double a = como_decimal(&izquierda), b = como_decimal(&derecha);
      salida->tipo = TIPO_TAZAS;
      switch (nodo->operador) {
      case '+': salida->tazas = a + b; break;
      case '-': salida->tazas = a - b; break;
      case '*': salida->tazas = a * b; break;
      case '/':
        if (b == 0.0) {
          reportar_error(FASE_EJECUCION, nodo->linea,
                         "No se puede repartir entre cero: división entre 0.");
          return 0;
        }
        salida->tazas = a / b;
        break;
      }
      return 1;
    }

    long long a = izquierda.porciones, b = derecha.porciones;
    switch (nodo->operador) {
    case '+': return resultado_porciones(a + b, nodo->linea, salida);
    case '-': return resultado_porciones(a - b, nodo->linea, salida);
    case '*': return resultado_porciones(a * b, nodo->linea, salida);
    case '/':
      if (b == 0) {
        reportar_error(FASE_EJECUCION, nodo->linea,
                       "No se puede repartir entre cero: división entre 0.");
        return 0;
      }
      /* Division entera, como en C: 7 / 2 = 3 */
      return resultado_porciones(a / b, nodo->linea, salida);
    }
    return 0;
  }

  case NODO_NEGATIVO: {
    Valor interno;
    if (!evaluar(nodo->hijos[0], &interno)) return 0;
    if (interno.tipo == TIPO_TAZAS) {
      salida->tipo = TIPO_TAZAS;
      salida->tazas = -interno.tazas;
      return 1;
    }
    return resultado_porciones(-(long long)interno.porciones, nodo->linea, salida);
  }

  case NODO_FUNCION: {
    Valor argumento;
    if (!evaluar(nodo->hijos[0], &argumento)) return 0;
    if (nodo->funcion == FUNC_DEGUSTAR) {
      salida->tipo = TIPO_BETUN;
      salida->betun = copiar_texto(tipo_nombre(argumento.tipo));
      valor_liberar(&argumento);
      return 1;
    }
    /* rebanar: corta los decimales (2,9 -> 2) */
    if (argumento.tipo == TIPO_PORCIONES) {
      *salida = argumento;
      return 1;
    }
    if (argumento.tazas < (double)INT_MIN || argumento.tazas > (double)INT_MAX) {
      reportar_error(FASE_EJECUCION, nodo->linea,
                     "rebanar: el valor es demasiado grande para porciones.");
      return 0;
    }
    salida->tipo = TIPO_PORCIONES;
    salida->porciones = (int)argumento.tazas;
    return 1;
  }

  case NODO_LLAMADA:
    return llamar_preparacion(nodo, salida);

  default:
    break;
  }
  return 0;
}

static int ejecutar_declaracion(const Nodo *nodo) {
  Simbolo *simbolo = tabla_simbolo(mesa, nodo->simbolo);
  if (simbolo == NULL) return 0;
  if (nodo->num_hijos == 0) {
    /* Valor por defecto (se reinicia en cada llamada a una preparacion). */
    Valor defecto = valor_por_defecto(simbolo->tipo);
    valor_asignar(&simbolo->valor, &defecto);
    valor_liberar(&defecto);
    return 1;
  }
  Valor valor;
  if (!evaluar(nodo->hijos[0], &valor)) return 0;
  valor = convertir(valor, simbolo->tipo);
  valor_asignar(&simbolo->valor, &valor);
  valor_liberar(&valor);
  return 1;
}

static int ejecutar_asignacion(const Nodo *nodo) {
  Simbolo *simbolo = tabla_simbolo(mesa, nodo->simbolo);
  if (simbolo == NULL) return 0;

  Valor valor;
  if (!evaluar(nodo->hijos[0], &valor)) return 0;
  valor = convertir(valor, simbolo->tipo);
  Valor *actual = &simbolo->valor;
  int ok = 1;

  switch (nodo->asignacion) {
  case ASIG_VIERTE:
    valor_asignar(actual, &valor);
    break;

  case ASIG_ESPOLVOREA:
    if (actual->tipo == TIPO_BETUN) {
      /* Une textos: "Feliz cumple" + ", Ana" */
      size_t largo = strlen(actual->betun) + strlen(valor.betun);
      char *unido = malloc(largo + 1);
      if (unido == NULL) {
        fprintf(stderr, "Sin memoria\n");
        exit(EXIT_FAILURE);
      }
      strcpy(unido, actual->betun);
      strcat(unido, valor.betun);
      free(actual->betun);
      actual->betun = unido;
    } else if (actual->tipo == TIPO_TAZAS) {
      actual->tazas += valor.tazas;
    } else {
      Valor suma;
      ok = resultado_porciones((long long)actual->porciones + valor.porciones,
                               nodo->linea, &suma);
      if (ok) actual->porciones = suma.porciones;
    }
    break;

  case ASIG_RASPA:
    if (actual->tipo == TIPO_TAZAS) {
      actual->tazas -= valor.tazas;
    } else {
      Valor resta;
      ok = resultado_porciones((long long)actual->porciones - valor.porciones,
                               nodo->linea, &resta);
      if (ok) actual->porciones = resta.porciones;
    }
    break;
  }
  valor_liberar(&valor);
  return ok;
}

static int ejecutar_sirve(const Nodo *nodo) {
  Valor valor;
  if (!evaluar(nodo->hijos[0], &valor)) return 0;
  char texto[64];
  switch (valor.tipo) {
  case TIPO_BETUN:
    printf("%s\n", valor.betun);
    break;
  case TIPO_CHISPA:
    printf("%c\n", valor.chispa);
    break;
  default:
    valor_a_texto(&valor, texto, sizeof(texto));
    printf("%s\n", texto);
    break;
  }
  valor_liberar(&valor);
  return 1;
}

/* ------------------------------------------------------------------ */
/* Preparaciones (Regla 4)                                             */
/* ------------------------------------------------------------------ */

/*
 * Llama a una preparacion:
 *   1. calcula los argumentos (en el ambito de quien llama),
 *   2. los copia en los ingredientes,
 *   3. ejecuta los pasos hasta 'entrega' o 'fin',
 *   4. devuelve el valor entregado.
 */
static int llamar_preparacion(const Nodo *llamada, Valor *salida) {
  const Simbolo *preparacion = tabla_simbolo(mesa, llamada->simbolo);
  if (preparacion == NULL) return 0;
  const Nodo *definicion = preparacion->definicion;
  const Nodo *ingredientes = definicion->hijos[0];
  const Nodo *pasos = definicion->hijos[1];

  if (profundidad >= PROFUNDIDAD_MAXIMA) {
    reportar_error(FASE_EJECUCION, llamada->linea,
                   "demasiadas preparaciones abiertas una dentro de otra.");
    return 0;
  }

  /* 1. Argumentos */
  int cantidad = llamada->num_hijos;
  Valor argumentos[64];
  for (int i = 0; i < cantidad && i < 64; i++) {
    if (!evaluar(llamada->hijos[i], &argumentos[i])) {
      for (int j = 0; j < i; j++) valor_liberar(&argumentos[j]);
      return 0;
    }
  }

  /* 2. Ingredientes */
  for (int i = 0; i < cantidad && i < 64; i++) {
    const Nodo *ingrediente = ingredientes->hijos[i];
    Simbolo *simbolo = tabla_simbolo(mesa, ingrediente->simbolo);
    Valor convertido = convertir(argumentos[i], ingrediente->tipo_declarado);
    valor_asignar(&simbolo->valor, &convertido);
    valor_liberar(&argumentos[i]);
  }

  /* 3. Pasos */
  Valor entregado;
  memset(&entregado, 0, sizeof(entregado));
  entregado.tipo = TIPO_NADA;
  profundidad++;
  Resultado resultado =
      ejecutar_instrucciones(pasos->hijos, pasos->num_hijos, &entregado);
  profundidad--;
  if (resultado == EJ_ERROR) {
    valor_liberar(&entregado);
    return 0;
  }

  /* 4. Valor entregado */
  *salida = convertir(entregado, preparacion->tipo);
  return 1;
}

static int ejecutar_entrega(const Nodo *nodo, Valor *entregado) {
  if (nodo->num_hijos == 0) {
    entregado->tipo = TIPO_NADA;
    return 1;
  }
  return evaluar(nodo->hijos[0], entregado);
}

static Resultado ejecutar_instrucciones(Nodo *const *instrucciones, int cantidad,
                                        Valor *entregado) {
  for (int i = 0; i < cantidad; i++) {
    const Nodo *instruccion = instrucciones[i];
    int ok = 1;
    switch (instruccion->clase) {
    case NODO_DECLARACION: ok = ejecutar_declaracion(instruccion); break;
    case NODO_ASIGNACION: ok = ejecutar_asignacion(instruccion); break;
    case NODO_SIRVE: ok = ejecutar_sirve(instruccion); break;
    case NODO_LLAMADA: {
      /* Llamada sola como instruccion: el valor (si hay) se descarta. */
      Valor descartado;
      ok = llamar_preparacion(instruccion, &descartado);
      if (ok) valor_liberar(&descartado);
      break;
    }
    case NODO_ENTREGA:
      if (!ejecutar_entrega(instruccion, entregado)) return EJ_ERROR;
      return EJ_ENTREGA;
    case NODO_PREPARACION:
      /* Solo es la definicion: se ejecuta cuando alguien la llama. */
      break;
    default:
      break;
    }
    if (!ok) {
      return EJ_ERROR;
    }
  }
  return EJ_OK;
}

int hornear_receta(const Nodo *receta, TablaSimbolos *tabla) {
  mesa = tabla;
  profundidad = 0;
  Valor sin_uso;
  memset(&sin_uso, 0, sizeof(sin_uso));
  Resultado resultado =
      ejecutar_instrucciones(receta->hijos, receta->num_hijos, &sin_uso);
  valor_liberar(&sin_uso);
  fflush(stdout);
  return resultado != EJ_ERROR;
}
