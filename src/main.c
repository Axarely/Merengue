/*
 * main.c - Compilador del lenguaje Merengue.
 *
 * Fases:
 *   1. Analisis lexico      (lexico.l)       -> tabla de tokens
 *   2. Analisis sintactico  (sintactico.y)   -> AST (arbol y tabla AST)
 *   3. Analisis semantico   (semantico.c)    -> tabla de simbolos
 *   4. Ejecucion            (interprete.c)   -> resultado de la receta
 *
 * Uso:
 *   ./merengue receta.mrg          muestra todas las fases (modo presentacion)
 *   ./merengue -q receta.mrg       solo la salida del programa (y los errores)
 *   ./merengue -l -a -t -s ...     elige que mostrar (ver --ayuda)
 */
#include "ast.h"
#include "errores.h"
#include "formato.h"
#include "interprete.h"
#include "semantico.h"
#include "tabla_simbolos.h"
#include "tokens.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern FILE *yyin;
extern int yyparse(void);
extern int yylex_destroy(void);
extern Nodo *raiz_ast;

typedef struct {
  const char *archivo;
  int tokens;       /* -l */
  int arbol;        /* -a */
  int tabla_ast;    /* -t */
  int simbolos;     /* -s */
  int encabezados;  /* titulos de cada fase */
} Opciones;

static void mostrar_ayuda(const char *programa) {
  printf("Compilador de Merengue (.mrg)\n\n");
  printf("Uso: %s [opciones] receta.mrg\n\n", programa);
  printf("Sin opciones se muestran todas las fases (modo presentación).\n\n");
  printf("  -l   tabla de tokens (análisis léxico)\n");
  printf("  -a   árbol de sintaxis abstracta (AST)\n");
  printf("  -t   tabla AST\n");
  printf("  -s   tabla de símbolos\n");
  printf("  -q   solo la salida de la receta\n");
  printf("  -h   esta ayuda\n");
}

static int leer_opciones(int argc, char **argv, Opciones *op) {
  int eligio = 0, silencioso = 0;
  for (int i = 1; i < argc; i++) {
    const char *a = argv[i];
    if (strcmp(a, "-l") == 0) { op->tokens = 1; eligio = 1; }
    else if (strcmp(a, "-a") == 0) { op->arbol = 1; eligio = 1; }
    else if (strcmp(a, "-t") == 0) { op->tabla_ast = 1; eligio = 1; }
    else if (strcmp(a, "-s") == 0) { op->simbolos = 1; eligio = 1; }
    else if (strcmp(a, "-q") == 0) { silencioso = 1; }
    else if (strcmp(a, "-h") == 0 || strcmp(a, "--ayuda") == 0) { return 0; }
    else if (a[0] == '-' || op->archivo != NULL) {
      fprintf(stderr, "Opción no reconocida: %s\n\n", a);
      return 0;
    } else { op->archivo = a; }
  }
  if (op->archivo == NULL) {
    fprintf(stderr, "Falta el archivo de la receta (.mrg)\n\n");
    return 0;
  }
  if (!eligio && !silencioso) {
    op->tokens = op->arbol = op->tabla_ast = op->simbolos = 1;
  }
  op->encabezados = !silencioso;
  return 1;
}

static void titulo(const Opciones *op, const char *texto) {
  if (op->encabezados) {
    imprimir_titulo(texto);
  }
}

static void resultado(const Opciones *op, const char *formato, int numero) {
  if (op->encabezados) {
    printf("  -> ");
    printf(formato, numero);
    printf("\n");
  }
}

int main(int argc, char **argv) {
  Opciones op = {0};
  if (!leer_opciones(argc, argv, &op)) {
    mostrar_ayuda(argv[0]);
    return EXIT_FAILURE;
  }

  yyin = fopen(op.archivo, "r");
  if (yyin == NULL) {
    perror(op.archivo);
    return EXIT_FAILURE;
  }

  if (op.encabezados) {
    printf("Compilador Merengue - receta: %s\n", op.archivo);
  }

  /* ---- Fases 1 y 2: el parser pide los tokens al lexer ---- */
  yyparse();
  fclose(yyin);
  yylex_destroy();
  ast_numerar(raiz_ast);

  int estado = EXIT_SUCCESS;
  TablaSimbolos tabla;
  tabla_iniciar(&tabla);

  /* ---- FASE 1 ---- */
  titulo(&op, "FASE 1 - ANÁLISIS LÉXICO (tokens)");
  if (op.tokens) {
    tokens_imprimir();
  }
  errores_imprimir(FASE_LEXICA);
  resultado(&op, "%d errores léxicos", errores_en(FASE_LEXICA));

  /* ---- FASE 2 ---- */
  titulo(&op, "FASE 2 - ANÁLISIS SINTÁCTICO (AST)");
  errores_imprimir(FASE_SINTACTICA);
  int errores_previos = errores_en(FASE_LEXICA) + errores_en(FASE_SINTACTICA);
  if (errores_previos > 0 || raiz_ast == NULL) {
    resultado(&op, "%d errores sintácticos", errores_en(FASE_SINTACTICA));
    fprintf(stderr, "\nCompilación detenida: corrige los errores léxicos y "
                    "sintácticos.\n");
    estado = EXIT_FAILURE;
    goto fin;
  }
  resultado(&op, "Sintaxis correcta: %d instrucciones reconocidas",
            raiz_ast->num_hijos);
  if (op.arbol) {
    if (op.encabezados) {
      printf("\nÁrbol de sintaxis abstracta que construyó el parser:\n\n");
    }
    ast_imprimir_arbol(raiz_ast);
  }

  /* ---- FASE 3 ---- */
  titulo(&op, "FASE 3 - ANÁLISIS SEMÁNTICO");
  int errores_semanticos = analizar_semantica(raiz_ast, &tabla);

  /* El semantico "decora" el AST con el tipo de cada expresion. */
  if (op.tabla_ast) {
    if (op.encabezados) {
      printf("Tabla AST (el semántico ya le anotó el tipo a cada nodo):\n\n");
    }
    ast_imprimir_tabla(raiz_ast);
  }
  if (op.simbolos) {
    if (op.encabezados) {
      printf("\nTabla de símbolos (moldes en la mesa antes de hornear):\n\n");
    }
    tabla_imprimir(&tabla, 0);
  }
  if (op.encabezados) {
    printf("\n");
  }
  errores_imprimir(FASE_SEMANTICA);
  if (errores_semanticos == 0) {
    advertencias_imprimir();
  }
  if (errores_semanticos > 0) {
    resultado(&op, "%d errores semánticos", errores_semanticos);
    fprintf(stderr, "\nCompilación detenida: la mezcla tiene errores "
                    "semánticos.\n");
    estado = EXIT_FAILURE;
    goto fin;
  }
  resultado(&op, "Semántica correcta, advertencias: %d", total_advertencias());

  /* ---- FASE 4 ---- */
  titulo(&op, "FASE 4 - EJECUCIÓN (horneando la receta)");
  if (!hornear_receta(raiz_ast, &tabla)) {
    fprintf(stderr, "\nLa receta se quemó: la ejecución se detuvo.\n");
    estado = EXIT_FAILURE;
  }

  if (op.simbolos) {
    if (op.encabezados) {
      printf("\nTabla de símbolos con los valores finales de cada molde:\n\n");
    }
    tabla_imprimir(&tabla, 1);
  }
  if (op.encabezados && estado == EXIT_SUCCESS) {
    printf("\n¡Receta horneada con éxito!\n");
  }

fin:
  ast_liberar(raiz_ast);
  tabla_liberar(&tabla);
  tokens_liberar();
  errores_liberar();
  return estado;
}
