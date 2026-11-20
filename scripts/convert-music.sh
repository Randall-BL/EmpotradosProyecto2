#!/usr/bin/env bash
# Convierte las canciones de una carpeta (mp3, m4a, flac, ogg, wav...) al formato que lee
# el reproductor (sw/common/src/wav.c): WAV PCM de 16 bits con el titulo, el artista y
# el album en LIST/INFO (INAM, IART, IPRD). Por defecto estereo a 44.1 kHz.
#
# Metadatos y formato por cancion: si la entrada trae canciones.tsv, cada linea es
#   archivo<TAB>titulo<TAB>artista<TAB>album<TAB>hz<TAB>canales
# (archivo sin extension; hz = 8000, 16000, 44100 o 48000; canales = 1 o 2; columnas
# vacias = lo que traiga el archivo o el valor por defecto). Asi se demuestran las
# frecuencias del enunciado (8, 16 y 44.1 kHz) y se completan los metadatos que los MP3
# no traen. Las lineas que empiezan con # se ignoran.
#
# Uso:  scripts/convert-music.sh [entrada] [salida]
#       (por defecto Musica/ -> build/hps/music-sd/)
# Luego: scripts/make-sdimage.sh build/hps/music-sd   (dentro del contenedor hps)
#
# Requiere ffmpeg en el anfitrion (sudo apt install ffmpeg). La carpeta Musica/ no se
# versiona (.gitignore).
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
IN="${1:-$HERE/Musica}"
OUT="${2:-$HERE/build/hps/music-sd}"
command -v ffmpeg >/dev/null || { echo "ERROR: falta ffmpeg (sudo apt install ffmpeg)" >&2; exit 1; }
[ -d "$IN" ] || { echo "ERROR: no existe $IN" >&2; exit 1; }
mkdir -p "$OUT"
TSV="$IN/canciones.tsv"

# meta <archivo-sin-ext> <columna 2..6>: valor de canciones.tsv o vacio
meta() {
    [ -f "$TSV" ] || return 0
    awk -F'\t' -v k="$1" -v c="$2" '$0 !~ /^#/ && $1 == k { print $c; exit }' "$TSV"
}

i=0
shopt -s nullglob nocaseglob
for f in "$IN"/*.{mp3,m4a,aac,flac,ogg,opus,wav}; do
    i=$((i + 1))
    base="$(basename "${f%.*}")"
    title="$(ffprobe -v error -show_entries format_tags=title -of default=nw=1:nk=1 "$f" || true)"
    artist="$(ffprobe -v error -show_entries format_tags=artist -of default=nw=1:nk=1 "$f" || true)"
    album="$(ffprobe -v error -show_entries format_tags=album -of default=nw=1:nk=1 "$f" || true)"
    v="$(meta "$base" 2)"; [ -z "$v" ] || title="$v"
    v="$(meta "$base" 3)"; [ -z "$v" ] || artist="$v"
    v="$(meta "$base" 4)"; [ -z "$v" ] || album="$v"
    hz="$(meta "$base" 5)"; hz="${hz:-44100}"
    ch="$(meta "$base" 6)"; ch="${ch:-2}"
    case "$hz" in 8000|16000|44100|48000) ;; *) echo "ERROR: $base: $hz Hz no soportado" >&2; exit 1 ;; esac
    case "$ch" in 1|2) ;; *) echo "ERROR: $base: $ch canales no soportado" >&2; exit 1 ;; esac
    [ -n "$title" ] || title="$base"
    # Pantalla y consola solo muestran ASCII: "Millón" -> "Millon".
    title="$(printf '%s' "$title" | iconv -f utf-8 -t ascii//TRANSLIT 2>/dev/null || printf '%s' "$title")"
    artist="$(printf '%s' "$artist" | iconv -f utf-8 -t ascii//TRANSLIT 2>/dev/null || printf '%s' "$artist")"
    album="$(printf '%s' "$album" | iconv -f utf-8 -t ascii//TRANSLIT 2>/dev/null || printf '%s' "$album")"
    # Nombre en ASCII simple para la FAT: NN_nombre.wav
    safe="$(printf '%s' "$base" | iconv -f utf-8 -t ascii//TRANSLIT 2>/dev/null | tr -c 'A-Za-z0-9_-' '_' | cut -c1-40)"
    dst="$OUT/$(printf '%02d' "$i")_${safe}.wav"
    echo ">> $base -> $(basename "$dst") ($hz Hz, $ch ch)"
    ffmpeg -nostdin -loglevel error -y -i "$f" -vn -map_metadata -1 \
        -ac "$ch" -ar "$hz" -c:a pcm_s16le \
        -metadata title="$title" ${artist:+-metadata artist="$artist"} \
        ${album:+-metadata album="$album"} "$dst"
done
[ "$i" -gt 0 ] || { echo "ERROR: no hay canciones en $IN" >&2; exit 1; }
echo ">> $i canciones en $OUT ($(du -sh "$OUT" | cut -f1))"
[ "$i" -ge 10 ] || echo "AVISO: el enunciado pide al menos 10 canciones (hay $i)" >&2
