#include "semantico.h"
#include "errores.h"

#include <stdio.h>
#include <string.h>

static TablaSimbolos *mesa;          /* tabla de simbolos que se esta llenando */
static int linea_ultima_declaracion; /* para "un solo molde por linea" */

/* Preparacion que se esta revisando (NULL = receta principal). */
static Nodo *preparacion_actual;
static int entregas_encontradas;

static void analizar_instruccion(Nodo *instruccion);

/* ------------------------------------------------------------------ */
/* Reglas de compatibilidad (Regla 3 del documento)                    */
/* ------------------------------------------------------------------ */

/*
 * 1 si un valor de tipo "origen" se puede guardar en un molde de tipo
 * "destino". Unica conversion automatica: porciones -> tazas.
 */
static int tipos_compatibles(TipoDato destino, TipoDato origen) {
  if (destino == TIPO_NADA || origen == TIPO_NADA) return 0;
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
/* Llamadas a preparaciones (Regla 4)                                  */
/* ------------------------------------------------------------------ */

static TipoDato analizar_expresion(Nodo *nodo);

/*
 * Revisa  nombre(arg y arg ...).
 * como_valor = 1 cuando la llamada esta dentro de una expresion (entonces
 * la preparacion tiene que rendir algo); 0 cuando es una instruccion sola.
 */
static TipoDato analizar_llamada(Nodo *nodo, int como_valor) {
  /* Los argumentos se revisan siempre, aunque la preparacion no exista. */
  TipoDato tipos_argumentos[64];
  for (int i = 0; i < nodo->num_hijos; i++) {
    TipoDato tipo = analizar_expresion(nodo->hijos[i]);
    if (i < 64) tipos_argumentos[i] = tipo;
  }
  nodo->tipo_dato = TIPO_DESCONOCIDO;

  int posicion = tabla_buscar(mesa, nodo->nombre);
  if (posicion < 0) {
    if (preparacion_actual != NULL &&
        strcmp(preparacion_actual->nombre, nodo->nombre) == 0) {
      reportar_error(FASE_SEMANTICA, nodo->linea,
                     "La preparación '%s' no puede usarse a sí misma.",
                     nodo->nombre);
    } else {
      reportar_error(FASE_SEMANTICA, nodo->linea,
                     "Esa preparación no está en el recetario: '%s' no fue "
                     "declarada (declárala antes con: preparacion %s(...) "
                     "rinde <tipo>: ... fin.)",
                     nodo->nombre, nodo->nombre);
    }
    return nodo->tipo_dato;
  }

  Simbolo *simbolo = tabla_simbolo(mesa, posicion);
  if (simbolo->categoria != CAT_PREPARACION) {
    reportar_error(FASE_SEMANTICA, nodo->linea,
                   "'%s' es un %s, no una preparación: no se puede usar con "
                   "paréntesis.",
                   nodo->nombre, categoria_nombre(simbolo->categoria));
    return nodo->tipo_dato;
  }
  nodo->simbolo = posicion;
  simbolo->usos++;

  const Nodo *definicion = simbolo->definicion;
  const Nodo *ingredientes = definicion->hijos[0];

  if (nodo->num_hijos != ingredientes->num_hijos) {
    reportar_error(FASE_SEMANTICA, nodo->linea,
                   "La preparación '%s' lleva %d ingrediente%s y se le "
                   "dieron %d.",
                   nodo->nombre, ingredientes->num_hijos,
                   ingredientes->num_hijos == 1 ? "" : "s", nodo->num_hijos);
  } else {
    for (int i = 0; i < nodo->num_hijos && i < 64; i++) {
      const Nodo *parametro = ingredientes->hijos[i];
      TipoDato recibido = tipos_argumentos[i];
      if (recibido == TIPO_DESCONOCIDO) continue;
      if (!tipos_compatibles(parametro->tipo_declarado, recibido)) {
        reportar_error(FASE_SEMANTICA, nodo->linea,
                       "La mezcla se cortó: el ingrediente %d de '%s' (%s) es "
                       "%s y se le dio %s.",
                       i + 1, nodo->nombre, parametro->nombre,
                       tipo_nombre(parametro->tipo_declarado),
                       tipo_nombre(recibido));
      }
    }
  }

  if (simbolo->tipo == TIPO_NADA && como_valor) {
    reportar_error(FASE_SEMANTICA, nodo->linea,
                   "La preparación '%s' rinde nada: no se puede usar como "
                   "valor.",
                   nodo->nombre);
    return nodo->tipo_dato;
  }
  nodo->tipo_dato = simbolo->tipo;
  return nodo->tipo_dato;
}

/* ------------------------------------------------------------------ */
/* Expresiones                                                         */
/* ------------------------------------------------------------------ */

static TipoDato analizar_expresion(Nodo *nodo) {
  switch (nodo->clase) {
  case NODO_LITERAL:
    return nodo->tipo_dato; /* el parser ya sabe el tipo de un literal */

  case NODO_IDENTIFICADOR: {
    int posicion = tabla_buscar(mesa, nodo->nombre);
    Simbolo *simbolo = tabla_simbolo(mesa, posicion);
    nodo->tipo_dato = TIPO_DESCONOCIDO;
    if (simbolo == NULL) {
      reportar_error(FASE_SEMANTICA, nodo->linea,
                     "Ese molde no está en la mesa: '%s' no fue declarado "
                     "(declara primero con: molde <tipo> %s.)",
                     nodo->nombre, nodo->nombre);
    } else if (simbolo->categoria == CAT_PREPARACION) {
      reportar_error(FASE_SEMANTICA, nodo->linea,
                     "'%s' es una preparación: para usarla escribe %s(...).",
                     nodo->nombre, nodo->nombre);
    } else {
      simbolo->usos++;
      nodo->simbolo = posicion;
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

  case NODO_LLAMADA:
    return analizar_llamada(nodo, 1);

  default:
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

  Simbolo *anterior = tabla_simbolo(mesa, tabla_buscar_local(mesa, nodo->nombre));
  if (anterior != NULL) {
    if (anterior->categoria == CAT_INGREDIENTE) {
      reportar_error(FASE_SEMANTICA, nodo->linea,
                     "Ese molde ya existe: '%s' ya es un ingrediente de '%s'.",
                     nodo->nombre, preparacion_actual->nombre);
    } else if (anterior->categoria == CAT_PREPARACION) {
      reportar_error(FASE_SEMANTICA, nodo->linea,
                     "Ese molde ya existe: '%s' ya es el nombre de una "
                     "preparación (línea %d).",
                     nodo->nombre, anterior->linea_declaracion);
    } else {
      reportar_error(FASE_SEMANTICA, nodo->linea,
                     "Ese molde ya existe: '%s' ya fue preparado en la línea %d.",
                     nodo->nombre, anterior->linea_declaracion);
    }
    return;
  }
  if (preparacion_actual != NULL && tabla_buscar_global(mesa, nodo->nombre) >= 0) {
    reportar_advertencia(nodo->linea,
                         "el molde '%s' de '%s' tapa al molde global del mismo "
                         "nombre.",
                         nodo->nombre, preparacion_actual->nombre);
  }
  nodo->simbolo = tabla_insertar(mesa, nodo->nombre, nodo->tipo_declarado,
                                 nodo->es_secreto ? CAT_SECRETO : CAT_MOLDE,
                                 nodo->linea, valor_inicial != NULL);
}

/* Regla 2: vierte / espolvorea / raspa. */
static void analizar_asignacion(Nodo *nodo) {
  TipoDato tipo_valor = analizar_expresion(nodo->hijos[0]);

  int posicion = tabla_buscar(mesa, nodo->nombre);
  Simbolo *simbolo = tabla_simbolo(mesa, posicion);
  if (simbolo == NULL) {
    reportar_error(FASE_SEMANTICA, nodo->linea,
                   "Ese molde no está en la mesa: '%s' no fue declarado.",
                   nodo->nombre);
    return;
  }
  if (simbolo->categoria == CAT_PREPARACION) {
    reportar_error(FASE_SEMANTICA, nodo->linea,
                   "'%s' es una preparación: no se le puede verter un valor.",
                   nodo->nombre);
    return;
  }
  nodo->simbolo = posicion;
  nodo->tipo_dato = simbolo->tipo;
  simbolo->modificaciones++;

  if (simbolo->categoria == CAT_SECRETO) {
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

/* entrega <valor>.   /   entrega. */
static void analizar_entrega(Nodo *nodo) {
  Nodo *valor = nodo->num_hijos > 0 ? nodo->hijos[0] : NULL;
  TipoDato tipo_valor = valor != NULL ? analizar_expresion(valor) : TIPO_NADA;

  if (preparacion_actual == NULL) {
    reportar_error(FASE_SEMANTICA, nodo->linea,
                   "'entrega' solo se usa dentro de una preparación.");
    return;
  }
  entregas_encontradas++;
  TipoDato rinde = preparacion_actual->tipo_declarado;
  nodo->tipo_dato = rinde;

  if (rinde == TIPO_NADA) {
    if (valor != NULL) {
      reportar_error(FASE_SEMANTICA, nodo->linea,
                     "La preparación '%s' rinde nada: se termina con  entrega.  "
                     "sin valor.",
                     preparacion_actual->nombre);
    }
    return;
  }
  if (valor == NULL) {
    reportar_error(FASE_SEMANTICA, nodo->linea,
                   "La preparación '%s' rinde %s: hay que entregar un valor "
                   "(entrega <valor>.).",
                   preparacion_actual->nombre, tipo_nombre(rinde));
    return;
  }
  if (tipo_valor != TIPO_DESCONOCIDO && !tipos_compatibles(rinde, tipo_valor)) {
    if (rinde == TIPO_PORCIONES && tipo_valor == TIPO_TAZAS) {
      reportar_error(FASE_SEMANTICA, nodo->linea,
                     "La mezcla se cortó: '%s' rinde porciones y se quiere "
                     "entregar tazas. Usa rebanar(...).",
                     preparacion_actual->nombre);
    } else {
      reportar_error(FASE_SEMANTICA, nodo->linea,
                     "La mezcla se cortó: '%s' rinde %s y se quiere entregar %s.",
                     preparacion_actual->nombre, tipo_nombre(rinde),
                     tipo_nombre(tipo_valor));
    }
  }
}

/* Regla 4: preparacion nombre(ingredientes) rinde <tipo>: pasos fin. */
static void analizar_preparacion(Nodo *nodo) {
  if (preparacion_actual != NULL) {
    reportar_error(FASE_SEMANTICA, nodo->linea,
                   "No se puede declarar la preparación '%s' dentro de '%s': "
                   "las preparaciones van en la receta principal.",
                   nodo->nombre, preparacion_actual->nombre);
    return;
  }
  Simbolo *anterior = tabla_simbolo(mesa, tabla_buscar_global(mesa, nodo->nombre));
  if (anterior != NULL) {
    reportar_error(FASE_SEMANTICA, nodo->linea,
                   "Ese nombre ya existe: '%s' ya es un %s (línea %d).",
                   nodo->nombre, categoria_nombre(anterior->categoria),
                   anterior->linea_declaracion);
    return;
  }

  Nodo *ingredientes = nodo->hijos[0];
  Nodo *pasos = nodo->hijos[1];
  if (ingredientes->num_hijos > 20) {
    reportar_error(FASE_SEMANTICA, nodo->linea,
                   "La preparación '%s' tiene %d ingredientes (máximo 20).",
                   nodo->nombre, ingredientes->num_hijos);
    return;
  }

  /* Entra al ambito de la preparacion: sus ingredientes y moldes son locales. */
  tabla_entrar_ambito(mesa, nodo->nombre);
  preparacion_actual = nodo;
  entregas_encontradas = 0;
  int linea_guardada = linea_ultima_declaracion;
  linea_ultima_declaracion = 0;

  for (int i = 0; i < ingredientes->num_hijos; i++) {
    Nodo *ingrediente = ingredientes->hijos[i];
    if (tabla_buscar_local(mesa, ingrediente->nombre) >= 0) {
      reportar_error(FASE_SEMANTICA, ingrediente->linea,
                     "El ingrediente '%s' está repetido en '%s'.",
                     ingrediente->nombre, nodo->nombre);
      continue;
    }
    if (tabla_buscar_global(mesa, ingrediente->nombre) >= 0) {
      reportar_advertencia(ingrediente->linea,
                           "el ingrediente '%s' de '%s' tapa al molde global "
                           "del mismo nombre.",
                           ingrediente->nombre, nodo->nombre);
    }
    ingrediente->simbolo =
        tabla_insertar(mesa, ingrediente->nombre, ingrediente->tipo_declarado,
                       CAT_INGREDIENTE, ingrediente->linea, 1);
  }

  int aviso_codigo_muerto = 0;
  for (int i = 0; i < pasos->num_hijos; i++) {
    if (entregas_encontradas > 0 && !aviso_codigo_muerto) {
      reportar_advertencia(pasos->hijos[i]->linea,
                           "esta instrucción está después de 'entrega' en '%s' "
                           "y nunca se ejecuta.",
                           nodo->nombre);
      aviso_codigo_muerto = 1;
    }
    analizar_instruccion(pasos->hijos[i]);
  }

  if (nodo->tipo_declarado != TIPO_NADA && entregas_encontradas == 0) {
    reportar_error(FASE_SEMANTICA, nodo->linea,
                   "La preparación '%s' rinde %s pero nunca entrega nada "
                   "(falta  entrega <valor>.).",
                   nodo->nombre, tipo_nombre(nodo->tipo_declarado));
  }

  /* Avisos de ingredientes y moldes locales sin usar (antes de salir). */
  for (int i = 0; i < mesa->cantidad; i++) {
    Simbolo *s = &mesa->simbolos[i];
    if (s->activo && s->nivel == 1 && s->usos == 0) {
      reportar_advertencia(s->linea_declaracion,
                           "el %s '%s' de '%s' nunca se usa.",
                           categoria_nombre(s->categoria), s->nombre,
                           nodo->nombre);
    }
  }

  tabla_salir_ambito(mesa);
  preparacion_actual = NULL;
  linea_ultima_declaracion = linea_guardada;

  /* Se registra DESPUES de revisar los pasos: por eso no puede usarse
     a si misma (no hay condicionales, se repetiria para siempre). */
  int posicion = tabla_insertar(mesa, nodo->nombre, nodo->tipo_declarado,
                                CAT_PREPARACION, nodo->linea, 0);
  Simbolo *simbolo = tabla_simbolo(mesa, posicion);
  simbolo->num_ingredientes = ingredientes->num_hijos;
  simbolo->definicion = nodo;
  nodo->simbolo = posicion;
}

static void analizar_instruccion(Nodo *instruccion) {
  if (instruccion == NULL) return;
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
  case NODO_PREPARACION:
    analizar_preparacion(instruccion);
    break;
  case NODO_ENTREGA:
    analizar_entrega(instruccion);
    break;
  case NODO_LLAMADA:
    analizar_llamada(instruccion, 0);
    break;
  default:
    break;
  }
}

int analizar_semantica(Nodo *receta, TablaSimbolos *tabla) {
  int errores_antes = errores_en(FASE_SEMANTICA);
  mesa = tabla;
  linea_ultima_declaracion = 0;
  preparacion_actual = NULL;

  for (int i = 0; i < receta->num_hijos; i++) {
    analizar_instruccion(receta->hijos[i]);
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
    const Simbolo *s = &tabla->simbolos[i];
    if (s->nivel != 0 || s->usos > 0) continue;
    if (s->categoria == CAT_PREPARACION) {
      reportar_advertencia(s->linea_declaracion,
                           "la preparación '%s' nunca se usa.", s->nombre);
    } else {
      reportar_advertencia(s->linea_declaracion,
                           "el molde '%s' se preparó pero nunca se usó en "
                           "una expresión.",
                           s->nombre);
    }
  }

  return errores_en(FASE_SEMANTICA) - errores_antes;
}
