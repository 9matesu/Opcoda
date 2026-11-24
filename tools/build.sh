#!/usr/bin/env bash
#
# Compila e testa o Opcoda no macOS e no Linux. E' o equivalente de
# tools/build.ps1: escolhe o preset certo para o sistema operativo e usa o
# compilador da maquina, sem vcvars nem Visual Studio.
#
#   tools/build.sh              # release, VST3 + standalone
#   tools/build.sh dev          # debug + ASan + testes
#
set -euo pipefail

mode="${1:-release}"
case "$mode" in
    dev | release) ;;
    *)
        echo "uso: tools/build.sh [dev|release]" >&2
        exit 2
        ;;
esac

case "$(uname -s)" in
    Darwin) system="mac" ;;
    Linux) system="linux" ;;
    *)
        echo "Sistema nao suportado: $(uname -s). Use tools/build.ps1 no Windows." >&2
        exit 2
        ;;
esac

if [ "$mode" = dev ]; then
    preset="$system-dev"
    config="Debug"
else
    preset="$system"
    config="Release"
fi

cmake --preset "$preset"
cmake --build --preset "$preset" --config "$config"

if [ "$mode" = dev ]; then
    ctest --preset "$preset" -C "$config" --output-on-failure
fi