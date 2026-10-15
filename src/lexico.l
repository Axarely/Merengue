/*
 * lexico.l - Analizador LEXICO de Merengue (se procesa con Flex).
 *
 * Lee los caracteres del archivo .mrg y los agrupa en TOKENS:
 * palabras reservadas, tipos, nombres de moldes, literales y simbolos.
 * Cada token se anota en la tabla de tokens (tokens.c) y se entrega al
 * analizador sintactico (sintactico.y).
 *
 * Reglas lexicas de Merengue:
 *   - Las instrucciones terminan con punto '.'
 *   - Los decimales se escriben con coma: 2,5
 *   - Los comentarios empiezan con '~' (el remolino de la manga pastelera)
 *   - Un nombre empieza con letra, sin tildes ni n con tilde, maximo 30 caracteres
 *   - Los ingredientes de una preparacion se separan con 'y' (no con coma)
 */

%{
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "errores.h"
#include "sintactico.tab.h"
#include "tipos.h"
#include "tokens.h"

#define LARGO_MAXIMO_NOMBRE 30

/* Antes de cada accion: guarda la linea del token para el parser. */
#define YY_USER_ACTION yylloc.first_line = yylloc.last_line = yylineno;

#define REGISTRAR(categoria) tokens_registrar(yylineno, categoria, yytext)

/* Linea del token anterior al actual (para el aviso de "falta el punto"). */
int linea_token_anterior = 1;
static int linea_token_actual = 1;
#define ENTREGAR(token) do { linea_token_anterior = linea_token_actual; \
                             linea_token_actual = yylineno; \
                             return (token); } while (0)
%}

%option noyywrap
%option yylineno
%option nounput
%option noinput

LETRA        [A-Za-z]
DIGITO       [0-9]
NO_ASCII     [\x80-\xff]

%%

[ \t\r\n]+                  { /* espacios y saltos de linea: se ignoran */ }
"~"[^\n]*                   { /* comentario del pastelero: se ignora */ }

 /* ---------- Palabras reservadas ---------- */
"molde"                     { REGISTRAR("PALABRA_RESERVADA"); ENTREGAR(T_MOLDE); }
"secreto"                   { REGISTRAR("PALABRA_RESERVADA"); ENTREGAR(T_SECRETO); }
"con"                       { REGISTRAR("PALABRA_RESERVADA"); ENTREGAR(T_CON); }
"vierte"                    { REGISTRAR("PALABRA_RESERVADA"); ENTREGAR(T_VIERTE); }
"en"                        { REGISTRAR("PALABRA_RESERVADA"); ENTREGAR(T_EN); }
"espolvorea"                { REGISTRAR("PALABRA_RESERVADA"); ENTREGAR(T_ESPOLVOREA); }
"raspa"                     { REGISTRAR("PALABRA_RESERVADA"); ENTREGAR(T_RASPA); }
"a"                         { REGISTRAR("PALABRA_RESERVADA"); ENTREGAR(T_A); }
"sirve"                     { REGISTRAR("PALABRA_RESERVADA"); ENTREGAR(T_SIRVE); }
 /* Preparaciones (funciones del usuario) */
"preparacion"               { REGISTRAR("PALABRA_RESERVADA"); ENTREGAR(T_PREPARACION); }
"rinde"                     { REGISTRAR("PALABRA_RESERVADA"); ENTREGAR(T_RINDE); }
"nada"                      { REGISTRAR("PALABRA_RESERVADA"); ENTREGAR(T_NADA); }
"entrega"                   { REGISTRAR("PALABRA_RESERVADA"); ENTREGAR(T_ENTREGA); }
"fin"                       { REGISTRAR("PALABRA_RESERVADA"); ENTREGAR(T_FIN); }
"y"                         { REGISTRAR("SEPARADOR"); ENTREGAR(T_Y); }

"degustar"                  { REGISTRAR("FUNCION_INTEGRADA"); ENTREGAR(T_DEGUSTAR); }
"rebanar"                   { REGISTRAR("FUNCION_INTEGRADA"); ENTREGAR(T_REBANAR); }

 /* ---------- Tipos de dato ---------- */
"porciones"|"tazas"|"betun"|"chispa"|"punto" {
                              REGISTRAR("TIPO_DE_DATO");
                              yylval.texto = copiar_texto(yytext);
                              ENTREGAR(T_TIPO);
                            }

 /* ---------- Literales ---------- */
"dorado"                    { REGISTRAR("LITERAL_PUNTO"); yylval.entero = 1; ENTREGAR(T_LIT_PUNTO); }
"crudo"                     { REGISTRAR("LITERAL_PUNTO"); yylval.entero = 0; ENTREGAR(T_LIT_PUNTO); }

{DIGITO}+","{DIGITO}+       {
                              /* tazas: decimal con coma. Se cambia la coma por punto para strtod. */
                              REGISTRAR("LITERAL_TAZAS");
                              char copia[128];
                              snprintf(copia, sizeof(copia), "%s", yytext);
                              char *coma = strchr(copia, ',');
                              if (coma != NULL) *coma = '.';
                              yylval.decimal = strtod(copia, NULL);
                              ENTREGAR(T_LIT_TAZAS);
                            }

{DIGITO}+"."{DIGITO}+       {
                              reportar_error(FASE_LEXICA, yylineno,
                                "'%s': en Merengue los decimales se escriben con coma, por ejemplo %.*s,%s",
                                yytext, (int)(strchr(yytext, '.') - yytext), yytext,
                                strchr(yytext, '.') + 1);
                              REGISTRAR("ERROR_LEXICO");
                              char copia[128];
                              snprintf(copia, sizeof(copia), "%s", yytext);
                              yylval.decimal = strtod(copia, NULL);
                              ENTREGAR(T_LIT_TAZAS);
                            }

{DIGITO}+                   {
                              REGISTRAR("LITERAL_PORCIONES");
                              errno = 0;
                              long valor = strtol(yytext, NULL, 10);
                              if (errno == ERANGE || valor > 2147483647L) {
                                reportar_error(FASE_LEXICA, yylineno,
                                  "el número %s es demasiado grande para porciones", yytext);
                                valor = 0;
                              }
                              yylval.entero = (int)valor;
                              ENTREGAR(T_LIT_PORCIONES);
                            }

\"([^\\\"\n]|\\.)*\"        {
                              REGISTRAR("LITERAL_BETUN");
                              yylval.texto = betun_desde_texto(yytext);
                              ENTREGAR(T_LIT_BETUN);
                            }

\"([^\\\"\n]|\\.)*          {
                              reportar_error(FASE_LEXICA, yylineno,
                                "el betún %s no se cerró con comillas dobles", yytext);
                              REGISTRAR("ERROR_LEXICO");
                              yylval.texto = copiar_texto("");
                              ENTREGAR(T_LIT_BETUN);
                            }

\'([^'\\\n]|\\.)\'          {
                              REGISTRAR("LITERAL_CHISPA");
                              yylval.caracter = chispa_desde_texto(yytext);
                              ENTREGAR(T_LIT_CHISPA);
                            }

\'[^'\n]*\'                 {
                              reportar_error(FASE_LEXICA, yylineno,
                                "%s: una chispa lleva un solo carácter; para texto usa betun con comillas dobles",
                                yytext);
                              REGISTRAR("ERROR_LEXICO");
                              yylval.caracter = yyleng > 2 ? yytext[1] : ' ';
                              ENTREGAR(T_LIT_CHISPA);
                            }

 /* ---------- Nombres de moldes (identificadores) ---------- */
{LETRA}({LETRA}|{DIGITO}|_)* {
                              if (yyleng > LARGO_MAXIMO_NOMBRE) {
                                reportar_error(FASE_LEXICA, yylineno,
                                  "el nombre '%s' tiene %d caracteres (máximo %d)",
                                  yytext, (int)yyleng, LARGO_MAXIMO_NOMBRE);
                              }
                              REGISTRAR("IDENTIFICADOR");
                              yylval.texto = copiar_texto(yytext);
                              ENTREGAR(T_ID);
                            }

{DIGITO}+({LETRA}|_)({LETRA}|{DIGITO}|_)* {
                              reportar_error(FASE_LEXICA, yylineno,
                                "'%s': un nombre debe empezar con una letra", yytext);
                              REGISTRAR("ERROR_LEXICO");
                              yylval.texto = copiar_texto(yytext);
                              ENTREGAR(T_ID);
                            }

(_|{NO_ASCII}|{LETRA})({LETRA}|{DIGITO}|_|{NO_ASCII})* {
                              /* Solo llega aqui si tiene tildes, n con tilde o empieza con '_' */
                              if (yytext[0] == '_') {
                                reportar_error(FASE_LEXICA, yylineno,
                                  "'%s': un nombre debe empezar con una letra, no con '_'", yytext);
                              } else {
                                reportar_error(FASE_LEXICA, yylineno,
                                  "'%s': los nombres y palabras de Merengue no llevan tildes ni ñ",
                                  yytext);
                              }
                              REGISTRAR("ERROR_LEXICO");
                              yylval.texto = copiar_texto(yytext);
                              ENTREGAR(T_ID);
                            }

 /* ---------- Operadores y simbolos ---------- */
"+"|"-"|"*"|"/"             { REGISTRAR("OPERADOR"); ENTREGAR(yytext[0]); }
"("|")"                     { REGISTRAR("PARENTESIS"); ENTREGAR(yytext[0]); }
"."                         { REGISTRAR("FIN_INSTRUCCION"); ENTREGAR('.'); }
":"                         { REGISTRAR("INICIO_DE_PASOS"); ENTREGAR(':'); }

","                         {
                              reportar_error(FASE_LEXICA, yylineno,
                                "',' sola no se usa en Merengue (la coma es para decimales como 2,5): "
                                "los ingredientes se separan con 'y'");
                              REGISTRAR("ERROR_LEXICO");
                              ENTREGAR(T_Y);
                            }

";"                         {
                              reportar_error(FASE_LEXICA, yylineno,
                                "';' no se usa en Merengue: las instrucciones terminan con punto '.'");
                              REGISTRAR("ERROR_LEXICO");
                              ENTREGAR('.');
                            }

"="                         {
                              reportar_error(FASE_LEXICA, yylineno,
                                "'=' no existe en Merengue: para asignar usa  vierte <valor> en <molde>.");
                              REGISTRAR("ERROR_LEXICO");
                            }

.                           {
                              reportar_error(FASE_LEXICA, yylineno,
                                "carácter no válido '%s'", yytext);
                              REGISTRAR("ERROR_LEXICO");
                            }

%%
