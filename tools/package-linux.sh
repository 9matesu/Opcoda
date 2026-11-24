#!/usr/bin/env bash
#
# Empacota o build do Linux num .tar.gz com o VST3, o standalone e um
# install.sh que copia para ~/.vst3, ~/.local/bin e cria o lancador .desktop.
#
#   tools/package-linux.sh [build-dir] [dist-dir]
#
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${1:-$repo_root/build-linux}"
dist_dir="${2:-$repo_root/dist}"

version="$(sed -n 's/^project(Opcoda VERSION \([0-9][0-9.]*\).*/\1/p' "$repo_root/CMakeLists.txt" | head -n1)"
if [ -z "$version" ]; then
    echo "nao foi possivel ler a versao de CMakeLists.txt" >&2
    exit 1
fi

vst3="$(find "$build_dir" -name 'Opcoda.vst3' -print -quit)"
# A permissao de execucao e' aplicada no pacote (chmod) e na instalacao
# (install -m 755): um build copiado por zip ou por uma pasta partilhada chega
# sem o bit, e o script nao pode depender dele.
standalone="$(find "$build_dir" -type f -name 'Opcoda' -print -quit)"
if [ -z "$vst3" ] || [ -z "$standalone" ]; then
    echo "Opcoda.vst3 ou o executavel Opcoda nao encontrados em $build_dir" >&2
    exit 1
fi

stage="$(mktemp -d)"
trap 'rm -rf "$stage"' EXIT

pkg="$stage/Opcoda-$version"
mkdir -p "$pkg"
cp -R "$vst3" "$pkg/"
cp "$standalone" "$pkg/Opcoda"
chmod +x "$pkg/Opcoda"
cp "$repo_root/resources/icons/opcoda-icon-256.png" "$pkg/opcoda.png"

cat > "$pkg/install.sh" <<'INSTALL'
#!/usr/bin/env bash
#
# Instala o Opcoda na area do utilizador: VST3 em ~/.vst3, standalone em
# ~/.local/bin e lancador em ~/.local/share/applications. Nao precisa de root.
#
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
vst3_dst="${VST3_DIR:-$HOME/.vst3}"
bin_dst="${BIN_DIR:-$HOME/.local/bin}"
apps_dst="${APPS_DIR:-$HOME/.local/share/applications}"
icons_dst="${ICONS_DIR:-$HOME/.local/share/icons/hicolor/256x256/apps}"

mkdir -p "$vst3_dst" "$bin_dst" "$apps_dst" "$icons_dst"

rm -rf "$vst3_dst/Opcoda.vst3"
cp -R "$here/Opcoda.vst3" "$vst3_dst/"
install -m 755 "$here/Opcoda" "$bin_dst/opcoda"
install -m 644 "$here/opcoda.png" "$icons_dst/opcoda.png"

cat > "$apps_dst/opcoda.desktop" <<EOF
[Desktop Entry]
Type=Application
Name=Opcoda
Comment=Granular synthesizer that reads executables as sound material
Exec=$bin_dst/opcoda
Icon=opcoda
Terminal=false
Categories=AudioVideo;Audio;
EOF

update-desktop-database "$apps_dst" 2>/dev/null || true

echo "Opcoda installed:"
echo "  VST3       $vst3_dst/Opcoda.vst3"
echo "  standalone $bin_dst/opcoda"
echo "Restart your host to see the VST3."
INSTALL

chmod +x "$pkg/install.sh"

mkdir -p "$dist_dir"
out="$dist_dir/Opcoda-$version-Linux.tar.gz"
rm -f "$out"
tar -czf "$out" -C "$stage" "Opcoda-$version"

echo "$out"