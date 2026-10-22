/*
 * sintactico.y - Analizador SINTACTICO de Merengue (se procesa con Bison).
 *
 * Recibe los tokens del analizador lexico y comprueba que sigan la
 * gramatica del lenguaje. Por cada regla reconocida crea un nodo del
 * AST (ver ast.c). Este archivo NO revisa tipos ni si un molde existe:
 * eso es trabajo del analizador semantico (semantico.c).
 *
 * Gramatica (resumen):
 *
 *   receta       -> instrucciones
 *   instrucciones-> instrucciones instruccion | (vacio)
 *   instruccion  -> declaracion '.' | asignacion '.' | salida '.'
 *                 | preparacion | llamada '.' | entrega '.'
 *   preparacion  -> preparacion NOMBRE ( ingredientes ) rinde TIPO|nada : pasos fin .
 *   ingredientes -> TIPO NOMBRE (y TIPO NOMBRE)*  | (vacio)
 *   llamada      -> NOMBRE ( argumentos )      argumentos -> expresion (y expresion)*
 *   entrega      -> entrega expresion | entrega
 *   condicional  -> si expresion : pasos [sino si expresion : pasos]* [sino : pasos] fin .
 *   declaracion  -> molde [secreto] TIPO NOMBRE [con expresion]
 *   asignacion   -> vierte expresion en NOMBRE
 *                 | espolvorea expresion a NOMBRE
 *                 | raspa expresion a NOMBRE
 *   salida       -> sirve expresion
 *   expresion    -> expresion (+ - * /) expresion | - expresion
 *                 | ( expresion ) | degustar ( expresion )
 *                 | rebanar ( expresion ) | llamada | NOMBRE | literal
 *                 | expresion (mayor que | menor que | igual a | ...) expresion
 *                 | expresion o expresion | expresion y ademas expresion | no expresion
 */

%code requires {
#include "ast.h"
}

%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ast.h"
#include "errores.h"
#include "tipos.h"

int yylex(void);
void yyerror(const char *mensaje);

extern int linea_token_anterior;

/* Raiz del AST: la usa main.c cuando termina el analisis. */
Nodo *raiz_ast = NULL;

/* Bloques abiertos (preparaciones y si) para avisar si falta 'fin.'. */
#define MAXIMO_BLOQUES 64
static int bloques_abiertos = 0;
static const char *bloque_tipo[MAXIMO_BLOQUES];
static int bloque_linea[MAXIMO_BLOQUES];

static void abrir_bloque(const char *tipo, int linea) {
  if (bloques_abiertos < MAXIMO_BLOQUES) {
    bloque_tipo[bloques_abiertos] = tipo;
    bloque_linea[bloques_abiertos] = linea;
  }
  bloques_abiertos++;
}

static void cerrar_bloque(void) {
  if (bloques_abiertos > 0) bloques_abiertos--;
}
%}

%locations
%define parse.error custom

%union {
  int entero;
  double decimal;
  char caracter;
  char *texto;
  struct Nodo *nodo;
}

/* Palabras reservadas (el texto entre comillas se usa en los mensajes de error). */
%token T_MOLDE "molde"
%token T_SECRETO "secreto"
%token T_CON "con"
%token T_VIERTE "vierte"
%token T_EN "en"
%token T_ESPOLVOREA "espolvorea"
%token T_RASPA "raspa"
%token T_A "a"
%token T_SIRVE "sirve"
%token T_DEGUSTAR "degustar"
%token T_REBANAR "rebanar"
%token T_PREPARACION "preparacion"
%token T_RINDE "rinde"
%token T_NADA "nada"
%token T_ENTREGA "entrega"
%token T_FIN "fin"
%token T_Y "y"
%token T_SI "si"
%token T_SINO "sino"
%token T_MAYOR "mayor que"
%token T_MENOR "menor que"
%token T_MAYOR_IGUAL "mayor o igual que"
%token T_MENOR_IGUAL "menor o igual que"
%token T_IGUAL "igual a"
%token T_DISTINTO "distinto de"
%token T_O "o"
%token T_Y_ADEMAS "y ademas"
%token T_NO "no"

%token <texto>    T_TIPO          "tipo de dato"
%token <texto>    T_ID            "nombre de molde"
%token <entero>   T_LIT_PORCIONES "número entero (porciones)"
%token <decimal>  T_LIT_TAZAS     "número decimal (tazas)"
%token <texto>    T_LIT_BETUN     "texto (betun)"
%token <caracter> T_LIT_CHISPA    "carácter (chispa)"
%token <entero>   T_LIT_PUNTO     "dorado o crudo"

%type <nodo> instrucciones instruccion declaracion asignacion salida expresion
%type <nodo> preparacion ingredientes lista_ingredientes ingrediente pasos
%type <nodo> llamada argumentos lista_argumentos entrega
%type <nodo> condicional resto_si
%type <entero> tipo_que_rinde

/* Precedencia: de menor a mayor. */
%left T_O
%left T_Y_ADEMAS
%precedence T_NO
%nonassoc T_MAYOR T_MENOR T_MAYOR_IGUAL T_MENOR_IGUAL T_IGUAL T_DISTINTO
%left '+' '-'
%left '*' '/'
%precedence NEGATIVO

/* Si hay un error, Bison libera lo que descarta. */
%destructor { ast_liberar($$); } <nodo>
%destructor { free($$); } <texto>

%start receta

%%

receta
    : instrucciones                 { raiz_ast = $1; }
    ;

instrucciones
    : %empty                        { $$ = nodo_receta(); }
    | instrucciones instruccion     { $$ = $1; nodo_agregar_hijo($$, $2); }
    ;

instruccion
    : declaracion '.'               { $$ = $1; }
    | asignacion '.'                { $$ = $1; }
    | salida '.'                    { $$ = $1; }
    | preparacion                   { $$ = $1; }
    | llamada '.'                   { $$ = $1; /* p. ej.  decorar("Ana"). */ }
    | entrega '.'                   { $$ = $1; }
    | condicional                   { $$ = $1; }
    | error '.'                     { $$ = NULL; yyerrok; /* sigue con la siguiente instruccion */ }
    ;

declaracion
    : T_MOLDE T_TIPO T_ID
        { $$ = nodo_declaracion(@1.first_line, 0, tipo_desde_texto($2), $3, NULL);
          free($2); free($3); }
    | T_MOLDE T_TIPO T_ID T_CON expresion
        { $$ = nodo_declaracion(@1.first_line, 0, tipo_desde_texto($2), $3, $5);
          free($2); free($3); }
    | T_MOLDE T_SECRETO T_TIPO T_ID
        { /* Sintacticamente valido; el semantico avisa que le falta 'con'. */
          $$ = nodo_declaracion(@1.first_line, 1, tipo_desde_texto($3), $4, NULL);
          free($3); free($4); }
    | T_MOLDE T_SECRETO T_TIPO T_ID T_CON expresion
        { $$ = nodo_declaracion(@1.first_line, 1, tipo_desde_texto($3), $4, $6);
          free($3); free($4); }
    ;

asignacion
    : T_VIERTE expresion T_EN T_ID
        { $$ = nodo_asignacion(@1.first_line, ASIG_VIERTE, $2, $4); free($4); }
    | T_ESPOLVOREA expresion T_A T_ID
        { $$ = nodo_asignacion(@1.first_line, ASIG_ESPOLVOREA, $2, $4); free($4); }
    | T_RASPA expresion T_A T_ID
        { $$ = nodo_asignacion(@1.first_line, ASIG_RASPA, $2, $4); free($4); }
    ;

salida
    : T_SIRVE expresion             { $$ = nodo_sirve(@1.first_line, $2); }
    ;

/* ---------- Preparaciones (funciones) ---------- */

preparacion
    : T_PREPARACION T_ID '(' ingredientes ')' T_RINDE tipo_que_rinde ':'
        { abrir_bloque("la preparación", @1.first_line); }
      pasos T_FIN '.'
        { cerrar_bloque();
          $$ = nodo_preparacion(@1.first_line, $2, $4, (TipoDato)$7, $10); free($2); }
    ;

tipo_que_rinde
    : T_TIPO                        { $$ = tipo_desde_texto($1); free($1); }
    | T_NADA                        { $$ = TIPO_NADA; }
    ;

ingredientes
    : %empty                        { $$ = nodo_ingredientes(yylloc.first_line); }
    | lista_ingredientes            { $$ = $1; }
    ;

lista_ingredientes
    : ingrediente                   { $$ = nodo_ingredientes(@1.first_line); nodo_agregar_hijo($$, $1); }
    | lista_ingredientes T_Y ingrediente { $$ = $1; nodo_agregar_hijo($$, $3); }
    ;

ingrediente
    : T_TIPO T_ID
        { $$ = nodo_ingrediente(@1.first_line, tipo_desde_texto($1), $2); free($1); free($2); }
    ;

pasos
    : %empty                        { $$ = nodo_pasos(yylloc.first_line); }
    | pasos instruccion             { $$ = $1; nodo_agregar_hijo($$, $2); }
    ;

/* ---------- Condicionales (Regla 5) ---------- */

/*   si <condicion>:
 *       ...
 *   sino si <condicion>:     (opcional, se puede repetir)
 *       ...
 *   sino:                    (opcional)
 *       ...
 *   fin.
 */
condicional
    : T_SI expresion ':' { abrir_bloque("el si", @1.first_line); } pasos resto_si
        { cerrar_bloque();
          $$ = nodo_si(@1.first_line, $2, nodo_bloque(NODO_ENTONCES, $5), $6); }
    ;

resto_si
    : T_FIN '.'                     { $$ = NULL; }
    | T_SINO ':' pasos T_FIN '.'    { $$ = nodo_bloque(NODO_SINO, $3); }
    | T_SINO T_SI expresion ':' pasos resto_si
        { /* "sino si" = un si nuevo dentro del sino */
          Nodo *sino = nodo_pasos(@1.first_line);
          nodo_agregar_hijo(sino, nodo_si(@2.first_line, $3,
                                          nodo_bloque(NODO_ENTONCES, $5), $6));
          $$ = nodo_bloque(NODO_SINO, sino); }
    ;

entrega
    : T_ENTREGA expresion           { $$ = nodo_entrega(@1.first_line, $2); }
    | T_ENTREGA                     { $$ = nodo_entrega(@1.first_line, NULL); }
    ;

llamada
    : T_ID '(' argumentos ')'       { $$ = nodo_llamada(@1.first_line, $1, $3); free($1); }
    ;

argumentos
    : %empty                        { $$ = NULL; }
    | lista_argumentos              { $$ = $1; }
    ;

lista_argumentos
    : expresion                     { $$ = nodo_lista_temporal(); nodo_agregar_hijo($$, $1); }
    | lista_argumentos T_Y expresion { $$ = $1; nodo_agregar_hijo($$, $3); }
    ;

expresion
    : expresion '+' expresion       { $$ = nodo_operacion(@2.first_line, '+', $1, $3); }
    | expresion '-' expresion       { $$ = nodo_operacion(@2.first_line, '-', $1, $3); }
    | expresion '*' expresion       { $$ = nodo_operacion(@2.first_line, '*', $1, $3); }
    | expresion '/' expresion       { $$ = nodo_operacion(@2.first_line, '/', $1, $3); }
    | expresion T_MAYOR expresion       { $$ = nodo_comparacion(@2.first_line, COMP_MAYOR, $1, $3); }
    | expresion T_MENOR expresion       { $$ = nodo_comparacion(@2.first_line, COMP_MENOR, $1, $3); }
    | expresion T_MAYOR_IGUAL expresion { $$ = nodo_comparacion(@2.first_line, COMP_MAYOR_IGUAL, $1, $3); }
    | expresion T_MENOR_IGUAL expresion { $$ = nodo_comparacion(@2.first_line, COMP_MENOR_IGUAL, $1, $3); }
    | expresion T_IGUAL expresion       { $$ = nodo_comparacion(@2.first_line, COMP_IGUAL, $1, $3); }
    | expresion T_DISTINTO expresion    { $$ = nodo_comparacion(@2.first_line, COMP_DISTINTO, $1, $3); }
    | expresion T_O expresion           { $$ = nodo_logica(@2.first_line, LOGICA_O, $1, $3); }
    | expresion T_Y_ADEMAS expresion    { $$ = nodo_logica(@2.first_line, LOGICA_Y, $1, $3); }
    | T_NO expresion                    { $$ = nodo_no(@1.first_line, $2); }
    | '-' expresion %prec NEGATIVO  { $$ = nodo_negativo(@1.first_line, $2); }
    | '(' expresion ')'             { $$ = $2; /* los parentesis no crean nodo */ }
    | T_DEGUSTAR '(' expresion ')'  { $$ = nodo_funcion(@1.first_line, FUNC_DEGUSTAR, $3); }
    | T_REBANAR '(' expresion ')'   { $$ = nodo_funcion(@1.first_line, FUNC_REBANAR, $3); }
    | llamada                       { $$ = $1; }
    | T_ID                          { $$ = nodo_identificador(@1.first_line, $1); free($1); }
    | T_LIT_PORCIONES               { $$ = nodo_literal_porciones(@1.first_line, $1); }
    | T_LIT_TAZAS                   { $$ = nodo_literal_tazas(@1.first_line, $1); }
    | T_LIT_BETUN                   { $$ = nodo_literal_betun(@1.first_line, $1); free($1); }
    | T_LIT_CHISPA                  { $$ = nodo_literal_chispa(@1.first_line, $1); }
    | T_LIT_PUNTO                   { $$ = nodo_literal_punto(@1.first_line, $1); }
    ;

%%

/* Nombre legible de un token para los mensajes en espanol. */
static void nombre_legible(yysymbol_kind_t simbolo, char *destino, size_t tamano) {
  if (simbolo == YYSYMBOL_YYEOF) {
    snprintf(destino, tamano, "el fin del archivo");
    return;
  }
  if (simbolo == YYSYMBOL_YYUNDEF) {
    snprintf(destino, tamano, "un símbolo no válido");
    return;
  }
  const char *nombre = yysymbol_name(simbolo);
  if (nombre[0] == '"') {
    /* Alias de Bison: viene con comillas dobles; se quitan. */
    size_t largo = strlen(nombre);
    char limpio[64];
    snprintf(limpio, sizeof(limpio), "%.*s", (int)(largo - 2), nombre + 1);
    snprintf(destino, tamano, "%s", limpio);
    nombre = destino;
  }
  if (nombre[0] != '\'' && strchr(nombre, ' ') == NULL) {
    /* palabra reservada: se muestra entre comillas, p. ej. 'molde' */
    char palabra[64];
    snprintf(palabra, sizeof(palabra), "%s", nombre);
    snprintf(destino, tamano, "'%s'", palabra);
  } else if (nombre != destino) {
    snprintf(destino, tamano, "%s", nombre);         /* '.', '+', descripciones */
  }
}

/* Mensaje de error de sintaxis en espanol (parse.error custom). */
/* 1 si el simbolo es el caracter dado, p. ej. simbolo_es(s, "'+'"). */
static int simbolo_es(yysymbol_kind_t s, const char *nombre) {
  return strcmp(yysymbol_name(s), nombre) == 0;
}

/* Clasifica un token esperado para resumir la lista en el mensaje. */
static int es_operador(yysymbol_kind_t s) {
  return simbolo_es(s, "'+'") || simbolo_es(s, "'-'") || simbolo_es(s, "'*'") ||
         simbolo_es(s, "'/'") || s == YYSYMBOL_T_MAYOR || s == YYSYMBOL_T_MENOR ||
         s == YYSYMBOL_T_MAYOR_IGUAL || s == YYSYMBOL_T_MENOR_IGUAL ||
         s == YYSYMBOL_T_IGUAL || s == YYSYMBOL_T_DISTINTO || s == YYSYMBOL_T_O ||
         s == YYSYMBOL_T_Y_ADEMAS;
}

static int es_inicio_de_valor(yysymbol_kind_t s) {
  return s == YYSYMBOL_T_ID || s == YYSYMBOL_T_LIT_PORCIONES ||
         s == YYSYMBOL_T_LIT_TAZAS || s == YYSYMBOL_T_LIT_BETUN ||
         s == YYSYMBOL_T_LIT_CHISPA || s == YYSYMBOL_T_LIT_PUNTO ||
         simbolo_es(s, "'('") || s == YYSYMBOL_T_NO || s == YYSYMBOL_T_DEGUSTAR ||
         s == YYSYMBOL_T_REBANAR;
}

/* Mensaje de error de sintaxis en espanol (parse.error custom). */
static int yyreport_syntax_error(const yypcontext_t *contexto) {
  enum { MAXIMO = 64 };
  yysymbol_kind_t esperados[MAXIMO];
  int cantidad = yypcontext_expected_tokens(contexto, esperados, MAXIMO);
  if (cantidad < 0) cantidad = 0;
  yysymbol_kind_t encontrado = yypcontext_token(contexto);
  int linea = yypcontext_location(contexto)->first_line;

  int espera_punto = 0, operadores = 0, valores = 0;
  for (int i = 0; i < cantidad; i++) {
    if (simbolo_es(esperados[i], "'.'")) espera_punto = 1;
    if (es_operador(esperados[i]) && !simbolo_es(esperados[i], "'-'")) operadores++;
    if (es_inicio_de_valor(esperados[i])) valores++;
  }

  char mensaje[512];
  char nombre[96];
  nombre_legible(encontrado, nombre, sizeof(nombre));

  if (espera_punto && (encontrado == YYSYMBOL_YYEOF || linea > linea_token_anterior)) {
    /* El caso mas comun: se olvido el punto al final de la linea anterior. */
    reportar_error(FASE_SINTACTICA, linea_token_anterior,
                   "falta el punto '.' al final de la instrucción");
    return 0;
  }

  if (encontrado == YYSYMBOL_YYEOF && bloques_abiertos > 0) {
    int i = bloques_abiertos <= MAXIMO_BLOQUES ? bloques_abiertos - 1
                                               : MAXIMO_BLOQUES - 1;
    reportar_error(FASE_SINTACTICA, linea_token_anterior,
                   "falta 'fin.' para cerrar %s que empieza en la línea %d",
                   bloque_tipo[i], bloque_linea[i]);
    return 0;
  }
  if ((encontrado == YYSYMBOL_T_FIN || encontrado == YYSYMBOL_T_SINO) &&
      bloques_abiertos == 0) {
    reportar_error(FASE_SINTACTICA, linea,
                   "sobra '%s': no hay ninguna preparación ni ningún si abierto "
                   "(revisa su primera línea)",
                   encontrado == YYSYMBOL_T_FIN ? "fin" : "sino");
    return 0;
  }

  if (encontrado == YYSYMBOL_T_MAYOR || encontrado == YYSYMBOL_T_MENOR ||
      encontrado == YYSYMBOL_T_MAYOR_IGUAL || encontrado == YYSYMBOL_T_MENOR_IGUAL ||
      encontrado == YYSYMBOL_T_IGUAL || encontrado == YYSYMBOL_T_DISTINTO) {
    reportar_error(FASE_SINTACTICA, linea,
                   "no se pueden encadenar comparaciones; únelas con 'y ademas' "
                   "(por ejemplo: x mayor que 1 y ademas x menor que 5)");
    return 0;
  }

  /* Arma la lista de lo esperado, resumiendo operadores y valores. */
  const char *partes[MAXIMO + 2];
  char nombres[MAXIMO][96];
  int num_partes = 0;
  int resumir_valores = valores >= 3;
  /* '-' es operador si tambien se espera '+'; si no, es el signo negativo. */
  int menos_es_operador = 0;
  for (int i = 0; i < cantidad; i++) {
    if (simbolo_es(esperados[i], "'+'")) menos_es_operador = 1;
  }
  if (resumir_valores) partes[num_partes++] = "un valor (número, texto, molde...)";
  for (int i = 0; i < cantidad; i++) {
    yysymbol_kind_t s = esperados[i];
    if (simbolo_es(s, "'-'") && !menos_es_operador) {
      if (resumir_valores) continue; /* el signo negativo ya va en "un valor" */
    } else if (es_operador(s)) {
      continue;
    }
    if (resumir_valores && es_inicio_de_valor(s)) continue;
    nombre_legible(s, nombres[i], sizeof(nombres[i]));
    partes[num_partes++] = nombres[i];
  }
  if (operadores > 0) partes[num_partes++] = "un operador (+, -, mayor que...)";

  snprintf(mensaje, sizeof(mensaje), "no se esperaba %s", nombre);
  if (num_partes > 0) {
    strncat(mensaje, "; se esperaba ", sizeof(mensaje) - strlen(mensaje) - 1);
    for (int i = 0; i < num_partes; i++) {
      if (i > 0) {
        strncat(mensaje, i == num_partes - 1 ? " o " : ", ",
                sizeof(mensaje) - strlen(mensaje) - 1);
      }
      strncat(mensaje, partes[i], sizeof(mensaje) - strlen(mensaje) - 1);
    }
  }
  reportar_error(FASE_SINTACTICA, linea, "%s", mensaje);
  return 0;
}

void yyerror(const char *mensaje) {
  reportar_error(FASE_SINTACTICA, yylloc.first_line, "%s", mensaje);
}
