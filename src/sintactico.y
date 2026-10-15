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
 *   declaracion  -> molde [secreto] TIPO NOMBRE [con expresion]
 *   asignacion   -> vierte expresion en NOMBRE
 *                 | espolvorea expresion a NOMBRE
 *                 | raspa expresion a NOMBRE
 *   salida       -> sirve expresion
 *   expresion    -> expresion (+ - * /) expresion | - expresion
 *                 | ( expresion ) | degustar ( expresion )
 *                 | rebanar ( expresion ) | llamada | NOMBRE | literal
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

/* Preparaciones abiertas (para avisar si falta 'fin.'). */
static int preparaciones_abiertas = 0;
static int linea_preparacion_abierta = 0;
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
%type <entero> tipo_que_rinde

/* Precedencia: de menor a mayor. */
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
        { if (preparaciones_abiertas++ == 0) linea_preparacion_abierta = @1.first_line; }
      pasos T_FIN '.'
        { preparaciones_abiertas--;
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
static int yyreport_syntax_error(const yypcontext_t *contexto) {
  enum { MAXIMO = 8 };
  yysymbol_kind_t esperados[MAXIMO];
  int total = yypcontext_expected_tokens(contexto, NULL, 0);
  int cantidad = total <= MAXIMO
                     ? yypcontext_expected_tokens(contexto, esperados, MAXIMO)
                     : 0;
  yysymbol_kind_t encontrado = yypcontext_token(contexto);
  int linea = yypcontext_location(contexto)->first_line;

  int espera_punto = 0;
  for (int i = 0; i < cantidad; i++) {
    if (strcmp(yysymbol_name(esperados[i]), "'.'") == 0) {
      espera_punto = 1;
    }
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

  if (encontrado == YYSYMBOL_YYEOF && preparaciones_abiertas > 0) {
    reportar_error(FASE_SINTACTICA, linea_token_anterior,
                   "falta 'fin.' para cerrar la preparación que empieza en la "
                   "línea %d",
                   linea_preparacion_abierta);
    return 0;
  }
  if (encontrado == YYSYMBOL_T_FIN && preparaciones_abiertas == 0) {
    reportar_error(FASE_SINTACTICA, linea,
                   "sobra 'fin': no hay ninguna preparación abierta "
                   "(revisa el encabezado de la preparación)");
    return 0;
  }

  snprintf(mensaje, sizeof(mensaje), "no se esperaba %s", nombre);
  if (total > MAXIMO) {
    /* Muchas opciones: es el inicio de una expresion. */
    strncat(mensaje, "; se esperaba un valor (número, texto, molde, dorado/crudo...)",
            sizeof(mensaje) - strlen(mensaje) - 1);
  } else if (cantidad > 0) {
    strncat(mensaje, "; se esperaba ", sizeof(mensaje) - strlen(mensaje) - 1);
    for (int i = 0; i < cantidad; i++) {
      nombre_legible(esperados[i], nombre, sizeof(nombre));
      if (i > 0) {
        strncat(mensaje, i == cantidad - 1 ? " o " : ", ",
                sizeof(mensaje) - strlen(mensaje) - 1);
      }
      strncat(mensaje, nombre, sizeof(mensaje) - strlen(mensaje) - 1);
    }
  }
  reportar_error(FASE_SINTACTICA, linea, "%s", mensaje);
  return 0;
}

void yyerror(const char *mensaje) {
  reportar_error(FASE_SINTACTICA, yylloc.first_line, "%s", mensaje);
}
