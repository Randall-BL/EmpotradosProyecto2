#!/usr/bin/env bash
# Convierte las canciones de una carpeta (mp3, m4a, flac, ogg, wav...) al formato que lee
# el reproductor (sw/common/src/wav.c): WAV PCM de 16 bits, estereo, 44.1 kHz, con el
# titulo y el artista en LIST/INFO. Si el archivo no trae titulo, se usa el nombre.
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

i=0
shopt -s nullglob nocaseglob
for f in "$IN"/*.{mp3,m4a,aac,flac,ogg,opus,wav}; do
    i=$((i + 1))
    base="$(basename "${f%.*}")"
    title="$(ffprobe -v error -show_entries format_tags=title -of default=nw=1:nk=1 "$f" || true)"
    artist="$(ffprobe -v error -show_entries format_tags=artist -of default=nw=1:nk=1 "$f" || true)"
    [ -n "$title" ] || title="$base"
    # Pantalla y consola solo muestran ASCII: "Millón" -> "Millon".
    title="$(printf '%s' "$title" | iconv -f utf-8 -t ascii//TRANSLIT 2>/dev/null || printf '%s' "$title")"
    artist="$(printf '%s' "$artist" | iconv -f utf-8 -t ascii//TRANSLIT 2>/dev/null || printf '%s' "$artist")"
    # Nombre en ASCII simple para la FAT: NN_nombre.wav
    safe="$(printf '%s' "$base" | iconv -f utf-8 -t ascii//TRANSLIT 2>/dev/null | tr -c 'A-Za-z0-9_-' '_' | cut -c1-40)"
    dst="$OUT/$(printf '%02d' "$i")_${safe}.wav"
    echo ">> $base -> $(basename "$dst")"
    ffmpeg -nostdin -loglevel error -y -i "$f" -vn -map_metadata -1 \
        -ac 2 -ar 44100 -c:a pcm_s16le \
        -metadata title="$title" ${artist:+-metadata artist="$artist"} "$dst"
done
[ "$i" -gt 0 ] || { echo "ERROR: no hay canciones en $IN" >&2; exit 1; }
echo ">> $i canciones en $OUT ($(du -sh "$OUT" | cut -f1))"
[ "$i" -ge 10 ] || echo "AVISO: el enunciado pide al menos 10 canciones (hay $i)" >&2
