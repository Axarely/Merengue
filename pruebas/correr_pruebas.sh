#!/bin/sh
# Corre cada receta de ejemplos/ con -q y compara la salida con pruebas/esperado/.
# Uso: sh pruebas/correr_pruebas.sh      (desde la carpeta del proyecto)
#      sh pruebas/correr_pruebas.sh --actualizar   (regenera las salidas esperadas)

pasaron=0
fallaron=0

for receta in ejemplos/*.mrg; do
  nombre=$(basename "$receta" .mrg)
  esperado="pruebas/esperado/$nombre.txt"
  obtenido=$(./merengue -q "$receta" 2>&1; echo "codigo de salida: $?")

  if [ "$1" = "--actualizar" ]; then
    printf '%s\n' "$obtenido" > "$esperado"
    echo "actualizado  $esperado"
    continue
  fi

  if [ "$(cat "$esperado")" = "$obtenido" ]; then
    echo "OK     $receta"
    pasaron=$((pasaron + 1))
  else
    echo "FALLO  $receta"
    printf '%s\n' "$obtenido" | diff "$esperado" - | head -20
    fallaron=$((fallaron + 1))
  fi
done

if [ "$1" != "--actualizar" ]; then
  echo ""
  echo "Pruebas: $pasaron correctas, $fallaron con fallos"
  [ "$fallaron" -eq 0 ]
fi
