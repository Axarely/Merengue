# Merengue: un lenguaje de programación de repostería

Merengue es un lenguaje donde **cada programa es una receta** y cada variable es un **molde** que se llena con ingredientes. Está inspirado en C (tipos fijos y reglas estrictas), pero sus palabras salen de la pastelería para que se lea como una receta en español.

Este repositorio contiene el compilador de Merengue, escrito en C con **Flex** (analizador léxico) y **Bison** (analizador sintáctico). El compilador revisa la receta en cuatro fases y después la ejecuta.

```
receta.mrg ──► Léxico ──► Sintáctico ──► Semántico ──► Ejecución
              (tokens)     (AST)        (tipos y       (resultado)
                                         tabla de
                                         símbolos)
```

## Características del lenguaje

- Cada instrucción termina con **punto** (`.`), como una oración de receta.
- Los decimales se escriben con **coma** (`2,5`), como se escriben en Costa Rica.
- No existe el `=`. El valor va **primero** y el molde **después**: `vierte 5 en huevos.`
- Los comentarios empiezan con `~`, el remolino de la manga pastelera.
- Los tipos son fijos y estrictos. Solo existe una conversión automática: de `porciones` a `tazas`.
- Los mensajes de error usan el tema de la repostería: *"Ese molde no está en la mesa"*, *"La mezcla se cortó"*.
- Los archivos usan la extensión `.mrg`.

## Reglas

### Nombres de moldes (identificadores)

1. Empiezan con una letra: `capa2` es válido, `2capa` no.
2. Solo usan letras, números y guion bajo, sin tildes ni ñ: `azucar_glas` sí, `azúcar glas` no.
3. Distinguen mayúsculas de minúsculas: `Crema` y `crema` son moldes distintos.
4. Tienen un máximo de 30 caracteres.
5. No pueden ser palabras reservadas.

**Palabras reservadas:** `molde`, `secreto`, `con`, `vierte`, `en`, `espolvorea`, `raspa`, `a`, `sirve`, `porciones`, `tazas`, `betun`, `chispa`, `punto`, `dorado`, `crudo`, `degustar`, `rebanar`.

### Regla 1: Declaración de variables

```
molde <tipo> <nombre>.
molde <tipo> <nombre> con <valor>.
molde secreto <tipo> <nombre> con <valor>.
```

- Se declara un solo molde por línea. No existe la forma de C `int a, b, c;`.
- Un molde debe declararse antes de usarse.
- No se puede declarar dos veces el mismo nombre.
- Sin `con`, el molde toma un valor por defecto: `0`, `0,0`, `""`, `' '` o `crudo`.
- `secreto` crea una **constante**. Debe recibir su valor con `con` y nunca puede cambiar.

### Regla 2: Asignación de variables

```
vierte <expresión> en <nombre>.       ~ reemplaza el valor   (=  en C)
espolvorea <expresión> a <nombre>.    ~ suma al valor        (+= en C)
raspa <expresión> a <nombre>.         ~ resta al valor       (-= en C)
```

- Un molde `secreto` no acepta `vierte`, `espolvorea` ni `raspa`.
- `espolvorea` y `raspa` funcionan con `porciones` y `tazas`. Con `betun`, `espolvorea` une textos.
- Las expresiones usan `+`, `-`, `*`, `/`, el signo negativo y paréntesis. `*` y `/` se calculan antes que `+` y `-`.
- Entre dos `porciones`, `/` hace división entera, como en C (`7 / 2` da `3`).

### Regla 3: Tipos de dato

| Tipo en Merengue | Qué guarda | Equivalente en C | Cómo se reconoce el valor | Ejemplos |
| --- | --- | --- | --- | --- |
| `porciones` | Números enteros | `int` | Solo dígitos | `3`, `0`, `190` |
| `tazas` | Números decimales | `float` | Dígitos con una coma decimal | `2,5`, `0,75` |
| `betun` | Texto | `char[]` | Entre comillas dobles | `"Feliz cumple"` |
| `chispa` | Un solo carácter | `char` | Entre comillas simples | `'A'`, `'7'` |
| `punto` | Verdadero o falso | `bool` | `dorado` o `crudo` | `dorado` |

Reglas de compatibilidad:

1. Un valor de `porciones` se puede guardar en un molde de `tazas`: `3` se guarda como `3,0`.
2. Un valor de `tazas` **no** entra en un molde de `porciones`. Hay que cortarle los decimales con `rebanar(...)`: `rebanar(2,9)` da `2`.
3. `betun`, `chispa` y `punto` no se mezclan con números.
4. `degustar(...)` devuelve el nombre del tipo de un valor como `betun`: `degustar(2,5)` da `"tazas"`.

### Instrucción de salida

```
sirve <expresión>.
```

Muestra un valor en pantalla. Por ejemplo, `sirve harina.` muestra `4,5`.

## Ejemplo

```
~ Receta: cupcakes de cumpleaños
molde secreto porciones huevos_por_tanda con 3.
molde porciones tandas con 2.
molde porciones huevos_total.
molde tazas harina con 2,5.
molde betun mensaje con "Feliz cumple".

vierte huevos_por_tanda * tandas en huevos_total.   ~ 6
vierte harina * tandas en harina.                   ~ 5,0
raspa 0,5 a harina.                                 ~ 4,5
espolvorea ", Ana" a mensaje.

sirve mensaje.
sirve huevos_total.
sirve harina.
```

Salida:

```
Feliz cumple, Ana
6
4,5
```

## Cómo compilar y ejecutar

Se necesitan `gcc`, `make`, `flex` y `bison`.

**Linux o WSL (Ubuntu):**

```bash
sudo apt install build-essential flex bison
make
./merengue ejemplos/cupcakes.mrg
```

**Windows con MSYS2:** abre la terminal *MSYS2 UCRT64*, instala con `pacman -S make gcc flex bison` y usa los mismos comandos. Si ves símbolos raros en lugar de tildes, ejecuta antes `chcp 65001`.

### Opciones

| Comando | Qué muestra |
| --- | --- |
| `./merengue receta.mrg` | Todas las fases (modo presentación) |
| `./merengue -q receta.mrg` | Solo la salida de la receta y los errores |
| `./merengue -l receta.mrg` | Tabla de tokens (análisis léxico) |
| `./merengue -a receta.mrg` | Árbol de sintaxis abstracta (AST) |
| `./merengue -t receta.mrg` | Tabla AST |
| `./merengue -s receta.mrg` | Tabla de símbolos |
| `make demo` | Corre la receta de los cupcakes con todas las fases |
| `make pruebas` | Corre todos los ejemplos y compara con la salida esperada |

## Las fases del compilador

| Fase | Archivo | Qué hace | Qué produce |
| --- | --- | --- | --- |
| 1. Análisis léxico | `src/lexico.l` | Agrupa los caracteres en tokens y rechaza los que no existen en Merengue | Tabla de tokens |
| 2. Análisis sintáctico | `src/sintactico.y` | Revisa que los tokens sigan la gramática y construye el AST | Árbol AST |
| 3. Análisis semántico | `src/semantico.c` | Revisa el significado: declaraciones, constantes y tipos. Anota el tipo de cada nodo del AST | Tabla AST y tabla de símbolos |
| 4. Ejecución | `src/interprete.c` | Recorre el AST y "hornea" la receta | Salida del programa |

Archivos de apoyo:

| Archivo | Contenido |
| --- | --- |
| `src/ast.c` | Nodos del AST, dibujo del árbol y tabla AST |
| `src/tabla_simbolos.c` | Tabla de símbolos ("la mesa de moldes") |
| `src/tokens.c` | Registro de tokens para la tabla léxica |
| `src/errores.c` | Errores y advertencias de cada fase |
| `src/tipos.c` | Los cinco tipos de dato y su equivalente en C |
| `src/formato.c` | Tablas y formato de decimales con coma |
| `src/main.c` | Une las fases y lee las opciones |

### Gramática

```
receta        → instrucciones
instrucciones → instrucciones instruccion | ε
instruccion   → declaracion '.' | asignacion '.' | salida '.'
declaracion   → molde TIPO NOMBRE
              | molde TIPO NOMBRE con expresion
              | molde secreto TIPO NOMBRE
              | molde secreto TIPO NOMBRE con expresion
asignacion    → vierte expresion en NOMBRE
              | espolvorea expresion a NOMBRE
              | raspa expresion a NOMBRE
salida        → sirve expresion
expresion     → expresion '+' expresion | expresion '-' expresion
              | expresion '*' expresion | expresion '/' expresion
              | '-' expresion | '(' expresion ')'
              | degustar '(' expresion ')' | rebanar '(' expresion ')'
              | NOMBRE | PORCIONES | TAZAS | BETUN | CHISPA | PUNTO
```

La precedencia se declara en Bison con `%left '+' '-'`, `%left '*' '/'` y `%precedence NEGATIVO`. Los paréntesis no crean un nodo en el AST: su efecto queda en la forma del árbol.

La gramática acepta `molde secreto TIPO NOMBRE` sin `con` a propósito. Esa forma es sintácticamente correcta, pero el analizador semántico la rechaza con un mensaje claro.

### Errores que detecta cada fase

| Fase | Ejemplo | Mensaje |
| --- | --- | --- |
| Léxica | `molde tazas azúcar.` | los nombres no llevan tildes ni ñ |
| Léxica | `2.5` | los decimales se escriben con coma, por ejemplo 2,5 |
| Sintáctica | `molde porciones x con 3` (sin punto) | falta el punto '.' al final de la instrucción |
| Sintáctica | `vierte en harina 5.` | no se esperaba 'en'; se esperaba un valor |
| Semántica | usar un molde no declarado | Ese molde no está en la mesa |
| Semántica | declarar dos veces | Ese molde ya existe |
| Semántica | cambiar un `secreto` | No se puede cambiar la receta secreta |
| Semántica | `tazas` en un molde `porciones` | La mezcla se cortó (usa rebanar) |
| Semántica | `molde porciones uno con 1. molde porciones dos con 2.` | Se declara un solo molde por línea |
| Ejecución | dividir entre un molde que vale 0 | No se puede repartir entre cero |

Cada fase reporta todos los errores que encuentra, con su número de línea. El analizador sintáctico se recupera con la regla `error '.'` y sigue revisando la siguiente instrucción.

## Ejemplos incluidos

| Archivo | Qué demuestra |
| --- | --- |
| `ejemplos/cupcakes.mrg` | La receta del documento del lenguaje |
| `ejemplos/pastel_chocolate.mrg` | Todas las reglas: tipos, constantes, valores por defecto, precedencia, `rebanar` y `degustar` |
| `ejemplos/errores_lexicos.mrg` | Errores del analizador léxico |
| `ejemplos/errores_sintacticos.mrg` | Errores del analizador sintáctico |
| `ejemplos/errores_semanticos.mrg` | Errores del analizador semántico |
| `ejemplos/error_ejecucion.mrg` | Un error que solo aparece al ejecutar |

## Estructura del repositorio

```
merengue/
├── Makefile
├── README.md
├── src/          código del compilador (.l, .y, .c, .h)
├── ejemplos/     recetas de prueba (.mrg)
└── pruebas/      script de pruebas y salidas esperadas
```
