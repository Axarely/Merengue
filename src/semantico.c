#include "semantico.h"
#include "errores.h"

#include <stdio.h>
#include <string.h>

static TablaSimbolos *mesa; /* tabla de simbolos que se esta llenando */
static int linea_ultima_declaracion; /* para "un solo molde por linea" */

/* ------------------------------------------------------------------ */
/* Reglas de compatibilidad (Regla 3 del documento)                    */
/* ------------------------------------------------------------------ */

/*
 * 1 si un valor de tipo "origen" se puede guardar en un molde de tipo
 * "destino". Unica conversion automatica: porciones -> tazas.
 */
static int tipos_compatibles(TipoDato destino, TipoDato origen) {
  if (destino == origen) return 1;
  if (destino == TIPO_TAZAS && origen == TIPO_PORCIONES) return 1;
  return 0;
}

/* Reporta por que no combinan, con una pista cuando se puede. */
static void reportar_mezcla(int linea, const char *nombre, TipoDato destino,
                            TipoDato origen) {
  if (destino == TIPO_PORCIONES && origen == TIPO_TAZAS) {
    reportar_error(FASE_SEMANTICA, linea,
                   "La mezcla se cortó: '%s' es porciones y no acepta tazas "
                   "(se perderían los decimales). Usa rebanar(...) para cortar "
                   "los decimales.",
                   nombre);
    return;
  }
  reportar_error(FASE_SEMANTICA, linea,
                 "La mezcla se cortó: '%s' es %s y no acepta un valor de tipo %s.",
                 nombre, tipo_nombre(destino), tipo_nombre(origen));
}

/* 1 si el nodo es un literal numerico igual a cero (para la division). */
static int es_cero_literal(const Nodo *nodo) {
  if (nodo->clase != NODO_LITERAL) return 0;
  if (nodo->tipo_dato == TIPO_PORCIONES) return nodo->valor.porciones == 0;
  if (nodo->tipo_dato == TIPO_TAZAS) return nodo->valor.tazas == 0.0;
  return 0;
}

/* ------------------------------------------------------------------ */
/* Expresiones                                                         */
/* ------------------------------------------------------------------ */

static TipoDato analizar_expresion(Nodo *nodo) {
  switch (nodo->clase) {
  case NODO_LITERAL:
    return nodo->tipo_dato; /* el parser ya sabe el tipo de un literal */

  case NODO_IDENTIFICADOR: {
    Simbolo *simbolo = tabla_buscar(mesa, nodo->nombre);
    if (simbolo == NULL) {
      reportar_error(FASE_SEMANTICA, nodo->linea,
                     "Ese molde no está en la mesa: '%s' no fue declarado "
                     "(declara primero con: molde <tipo> %s.)",
                     nodo->nombre, nodo->nombre);
      nodo->tipo_dato = TIPO_DESCONOCIDO;
    } else {
      simbolo->usos++;
      nodo->tipo_dato = simbolo->tipo;
    }
    return nodo->tipo_dato;
  }

  case NODO_OPERACION: {
    TipoDato izquierda = analizar_expresion(nodo->hijos[0]);
    TipoDato derecha = analizar_expresion(nodo->hijos[1]);
    nodo->tipo_dato = TIPO_DESCONOCIDO;
    if (izquierda == TIPO_DESCONOCIDO || derecha == TIPO_DESCONOCIDO) {
      return nodo->tipo_dato; /* el error ya se reporto abajo */
    }
    if (!tipo_es_numerico(izquierda) || !tipo_es_numerico(derecha)) {
      reportar_error(FASE_SEMANTICA, nodo->linea,
                     "La mezcla se cortó: no se puede usar '%c' entre %s y %s "
                     "(los operadores solo funcionan con porciones y tazas).",
                     nodo->operador, tipo_nombre(izquierda), tipo_nombre(derecha));
      return nodo->tipo_dato;
    }
    if (nodo->operador == '/' && es_cero_literal(nodo->hijos[1])) {
      reportar_error(FASE_SEMANTICA, nodo->linea,
                     "No se puede repartir entre cero: división entre 0.");
      return nodo->tipo_dato;
    }
    /* Si alguno es tazas, el resultado es tazas; si no, porciones. */
    nodo->tipo_dato = (izquierda == TIPO_TAZAS || derecha == TIPO_TAZAS)
                          ? TIPO_TAZAS
                          : TIPO_PORCIONES;
    return nodo->tipo_dato;
  }

  case NODO_NEGATIVO: {
    TipoDato interno = analizar_expresion(nodo->hijos[0]);
    nodo->tipo_dato = TIPO_DESCONOCIDO;
    if (interno == TIPO_DESCONOCIDO) return nodo->tipo_dato;
    if (!tipo_es_numerico(interno)) {
      reportar_error(FASE_SEMANTICA, nodo->linea,
                     "La mezcla se cortó: el signo '-' no se puede usar con %s.",
                     tipo_nombre(interno));
      return nodo->tipo_dato;
    }
    nodo->tipo_dato = interno;
    return nodo->tipo_dato;
  }

  case NODO_FUNCION: {
    TipoDato argumento = analizar_expresion(nodo->hijos[0]);
    if (nodo->funcion == FUNC_DEGUSTAR) {
      /* degustar acepta cualquier tipo y devuelve su nombre como betun. */
      nodo->tipo_dato = TIPO_BETUN;
      return nodo->tipo_dato;
    }
    /* rebanar: corta los decimales -> porciones */
    nodo->tipo_dato = TIPO_PORCIONES;
    if (argumento != TIPO_DESCONOCIDO && !tipo_es_numerico(argumento)) {
      reportar_error(FASE_SEMANTICA, nodo->linea,
                     "La mezcla se cortó: rebanar(...) solo corta tazas o "
                     "porciones, no %s.",
                     tipo_nombre(argumento));
      nodo->tipo_dato = TIPO_DESCONOCIDO;
    }
    return nodo->tipo_dato;
  }

  case NODO_RECETA:
  case NODO_DECLARACION:
  case NODO_ASIGNACION:
  case NODO_SIRVE:
    break;
  }
  return TIPO_DESCONOCIDO;
}

/* ------------------------------------------------------------------ */
/* Instrucciones                                                       */
/* ------------------------------------------------------------------ */

/* Regla 1: molde [secreto] <tipo> <nombre> [con <valor>]. */
static void analizar_declaracion(Nodo *nodo) {
  Nodo *valor_inicial = nodo->num_hijos > 0 ? nodo->hijos[0] : NULL;

  /* El valor se revisa ANTES de agregar el molde a la mesa:
     asi "molde porciones x con x." da error (x todavia no existe). */
  if (valor_inicial != NULL) {
    TipoDato tipo_valor = analizar_expresion(valor_inicial);
    if (tipo_valor != TIPO_DESCONOCIDO &&
        !tipos_compatibles(nodo->tipo_declarado, tipo_valor)) {
      reportar_mezcla(nodo->linea, nodo->nombre, nodo->tipo_declarado, tipo_valor);
    }
  }

  if (nodo->linea == linea_ultima_declaracion) {
    reportar_error(FASE_SEMANTICA, nodo->linea,
                   "Se declara un solo molde por línea: '%s' debe ir en su "
                   "propia línea.",
                   nodo->nombre);
  }
  linea_ultima_declaracion = nodo->linea;

  if (nodo->es_secreto && valor_inicial == NULL) {
    reportar_error(FASE_SEMANTICA, nodo->linea,
                   "El molde secreto '%s' necesita su valor en la misma línea: "
                   "molde secreto %s %s con <valor>.",
                   nodo->nombre, tipo_nombre(nodo->tipo_declarado), nodo->nombre);
  }

  Simbolo *anterior = tabla_buscar(mesa, nodo->nombre);
  if (anterior != NULL) {
    reportar_error(FASE_SEMANTICA, nodo->linea,
                   "Ese molde ya existe: '%s' ya fue preparado en la línea %d.",
                   nodo->nombre, anterior->linea_declaracion);
    return;
  }
  tabla_insertar(mesa, nodo->nombre, nodo->tipo_declarado, nodo->es_secreto,
                 nodo->linea, valor_inicial != NULL);
}

/* Regla 2: vierte / espolvorea / raspa. */
static void analizar_asignacion(Nodo *nodo) {
  TipoDato tipo_valor = analizar_expresion(nodo->hijos[0]);

  Simbolo *simbolo = tabla_buscar(mesa, nodo->nombre);
  if (simbolo == NULL) {
    reportar_error(FASE_SEMANTICA, nodo->linea,
                   "Ese molde no está en la mesa: '%s' no fue declarado.",
                   nodo->nombre);
    return;
  }
  nodo->tipo_dato = simbolo->tipo;
  simbolo->modificaciones++;

  if (simbolo->es_secreto) {
    reportar_error(FASE_SEMANTICA, nodo->linea,
                   "No se puede cambiar la receta secreta: '%s' es un molde "
                   "secreto (constante).",
                   nodo->nombre);
    return;
  }
  if (tipo_valor == TIPO_DESCONOCIDO) {
    return; /* el error de la expresion ya se reporto */
  }

  switch (nodo->asignacion) {
  case ASIG_VIERTE:
    if (!tipos_compatibles(simbolo->tipo, tipo_valor)) {
      reportar_mezcla(nodo->linea, nodo->nombre, simbolo->tipo, tipo_valor);
    }
    break;

  case ASIG_ESPOLVOREA:
    if (simbolo->tipo == TIPO_BETUN) {
      if (tipo_valor != TIPO_BETUN) {
        reportar_error(FASE_SEMANTICA, nodo->linea,
                       "La mezcla se cortó: a un betun solo se le puede "
                       "espolvorear otro betun (texto), no %s.",
                       tipo_nombre(tipo_valor));
      }
    } else if (tipo_es_numerico(simbolo->tipo)) {
      if (!tipos_compatibles(simbolo->tipo, tipo_valor)) {
        reportar_mezcla(nodo->linea, nodo->nombre, simbolo->tipo, tipo_valor);
      }
    } else {
      reportar_error(FASE_SEMANTICA, nodo->linea,
                     "espolvorea solo funciona con porciones, tazas o betun; "
                     "'%s' es %s.",
                     nodo->nombre, tipo_nombre(simbolo->tipo));
    }
    break;

  case ASIG_RASPA:
    if (!tipo_es_numerico(simbolo->tipo)) {
      reportar_error(FASE_SEMANTICA, nodo->linea,
                     "raspa solo funciona con porciones y tazas; '%s' es %s.",
                     nodo->nombre, tipo_nombre(simbolo->tipo));
    } else if (!tipos_compatibles(simbolo->tipo, tipo_valor)) {
      reportar_mezcla(nodo->linea, nodo->nombre, simbolo->tipo, tipo_valor);
    }
    break;
  }
}

int analizar_semantica(Nodo *receta, TablaSimbolos *tabla) {
  int errores_antes = errores_en(FASE_SEMANTICA);
  mesa = tabla;
  linea_ultima_declaracion = 0;

  for (int i = 0; i < receta->num_hijos; i++) {
    Nodo *instruccion = receta->hijos[i];
    switch (instruccion->clase) {
    case NODO_DECLARACION:
      analizar_declaracion(instruccion);
      break;
    case NODO_ASIGNACION:
      analizar_asignacion(instruccion);
      break;
    case NODO_SIRVE:
      analizar_expresion(instruccion->hijos[0]);
      break;
    default:
      break;
    }
  }

  /* Advertencias: no detienen la compilacion. Si ya hay errores no se
     muestran, para no llenar la pantalla. */
  if (errores_en(FASE_SEMANTICA) > errores_antes) {
    return errores_en(FASE_SEMANTICA) - errores_antes;
  }
  if (receta->num_hijos == 0) {
    reportar_advertencia(1, "la receta está vacía.");
  }
  for (int i = 0; i < tabla->cantidad; i++) {
    if (tabla->simbolos[i].usos == 0) {
      reportar_advertencia(tabla->simbolos[i].linea_declaracion,
                           "el molde '%s' se preparó pero nunca se usó en "
                           "una expresión.",
                           tabla->simbolos[i].nombre);
    }
  }

  return errores_en(FASE_SEMANTICA) - errores_antes;
}
