#include "interprete.h"
#include "errores.h"
#include "formato.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static TablaSimbolos *mesa;

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
    case TIPO_DESCONOCIDO: return 0;
    }
    return 1;

  case NODO_IDENTIFICADOR: {
    Simbolo *simbolo = tabla_buscar(mesa, nodo->nombre);
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

  case NODO_RECETA:
  case NODO_DECLARACION:
  case NODO_ASIGNACION:
  case NODO_SIRVE:
    break;
  }
  return 0;
}

static int ejecutar_declaracion(const Nodo *nodo) {
  Simbolo *simbolo = tabla_buscar(mesa, nodo->nombre);
  if (simbolo == NULL) return 0;
  if (nodo->num_hijos == 0) {
    return 1; /* se queda con su valor por defecto */
  }
  Valor valor;
  if (!evaluar(nodo->hijos[0], &valor)) return 0;
  valor = convertir(valor, simbolo->tipo);
  valor_asignar(&simbolo->valor, &valor);
  valor_liberar(&valor);
  return 1;
}

static int ejecutar_asignacion(const Nodo *nodo) {
  Simbolo *simbolo = tabla_buscar(mesa, nodo->nombre);
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

int hornear_receta(const Nodo *receta, TablaSimbolos *tabla) {
  mesa = tabla;
  for (int i = 0; i < receta->num_hijos; i++) {
    const Nodo *instruccion = receta->hijos[i];
    int ok = 1;
    switch (instruccion->clase) {
    case NODO_DECLARACION: ok = ejecutar_declaracion(instruccion); break;
    case NODO_ASIGNACION: ok = ejecutar_asignacion(instruccion); break;
    case NODO_SIRVE: ok = ejecutar_sirve(instruccion); break;
    default: break;
    }
    if (!ok) {
      return 0;
    }
  }
  fflush(stdout);
  return 1;
}
