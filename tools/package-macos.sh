#!/usr/bin/env bash
#
# Empacota o build do macOS num .dmg com o VST3, o standalone e um atalho para
# /Applications. Nao assina nem notaria: o utilizador abre a primeira vez com
# Ctrl+clique (ou boto direito) -> Abrir, porque o Gatekeeper bloqueia binarios
# descarregados sem identidade de programador.
#
#   tools/package-macos.sh [build-dir] [dist-dir]
#
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${1:-$repo_root/build-mac}"
dist_dir="${2:-$repo_root/dist}"

version="$(sed -n 's/^project(Opcoda VERSION \([0-9][0-9.]*\).*/\1/p' "$repo_root/CMakeLists.txt" | head -n1)"
if [ -z "$version" ]; then
    echo "nao foi possivel ler a versao de CMakeLists.txt" >&2
    exit 1
fi

vst3="$(find "$build_dir" -name 'Opcoda.vst3' -print -quit)"
app="$(find "$build_dir" -name 'Opcoda.app' -print -quit)"
if [ -z "$vst3" ] || [ -z "$app" ]; then
    echo "Opcoda.vst3 ou Opcoda.app nao encontrados em $build_dir" >&2
    exit 1
fi

stage="$(mktemp -d)"
trap 'rm -rf "$stage"' EXIT

cp -R "$vst3" "$stage/"
cp -R "$app" "$stage/"
# arrastar para Aplicacoes e' o gesto que o utilizador conhece.
ln -s /Applications "$stage/Applications"

cat > "$stage/README.txt" <<'EOF'
Opcoda
======

Installation
------------
1. Drag Opcoda.vst3 to ~/Library/Audio/Plug-Ins/VST3 (create the folder if it
   does not exist). To install for every user, use
   /Library/Audio/Plug-Ins/VST3.
2. Drag Opcoda.app to Applications.
3. Open Opcoda once from Applications. If macOS blocks it, Ctrl-click (or
   right-click) the app and choose Open: this build is not notarized.

Opcoda is an instrument (a synthesizer). Put it on an instrument track and send
MIDI notes.
EOF

mkdir -p "$dist_dir"
out="$dist_dir/Opcoda-$version-macOS.dmg"
rm -f "$out"
hdiutil create -volname "Opcoda $version" -srcfolder "$stage" -ov -format UDZO "$out"

echo "$out"