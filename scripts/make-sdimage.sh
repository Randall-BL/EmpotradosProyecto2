#!/usr/bin/env bash
# Arma la imagen de la microSD del HPS (HP-03) en un archivo, sin root (sfdisk sobre
# el archivo + mtools para la FAT). Corre dentro del contenedor docker/hps.Dockerfile:
#   docker compose -f docker/compose.yml run --rm hps scripts/make-sdimage.sh [musica]
#
# Particiones (MBR):
#   1  FAT32  "REPRODUCTOR"  desde 16 MiB: boot.scr, zImage, de1_soc_audio.dtb,
#                            audio_player.rbf (si existe) y music/*.wav
#   2  tipo A2               1 MiB..9 MiB: u-boot-with-spl.sfp en crudo (la Boot ROM
#                            busca el preloader en la particion de tipo A2)
#
# [musica]: carpeta con los .wav (por defecto build/hps/music, ver gen-test-songs.py).
# Salida: build/hps/sdcard.img. Grabar en la SD (fuera del contenedor, con cuidado de
# elegir el dispositivo correcto):
#   sudo dd if=build/hps/sdcard.img of=/dev/sdX bs=4M conv=fsync status=progress
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
B="$HERE/build/hps"
OUT="$B/out"
MUSIC="${1:-$B/music}"
IMG="$B/sdcard.img"
FAT="$B/fat.img"
RBF="$HERE/hw/quartus/output_files/audio_player.rbf"

FAT_MIB="${FAT_MIB:-512}"
A2_START_MIB=1
A2_MIB=8
FAT_START_MIB=16

for f in u-boot-with-spl.sfp zImage de1_soc_audio.dtb boot.scr; do
    [ -f "$OUT/$f" ] || { echo "ERROR: falta $OUT/$f; corra scripts/build-hps-linux.sh" >&2; exit 1; }
done
[ -d "$MUSIC" ] || { echo "ERROR: no existe la carpeta de musica $MUSIC" >&2; exit 1; }

echo ">> FAT32 de $FAT_MIB MiB"
rm -f "$FAT"
truncate -s "${FAT_MIB}M" "$FAT"
mkfs.vfat -F 32 -n REPRODUCTOR "$FAT" >/dev/null
export MTOOLS_SKIP_CHECK=1
mcopy -i "$FAT" "$OUT/boot.scr" "$OUT/zImage" "$OUT/de1_soc_audio.dtb" ::/
if [ -f "$RBF" ]; then
    mcopy -i "$FAT" "$RBF" ::/audio_player.rbf
    echo "   con audio_player.rbf (U-Boot configura la FPGA al arrancar)"
else
    echo "   sin audio_player.rbf (la FPGA se programa por JTAG)"
fi
mmd -i "$FAT" ::/music
n=0
for w in "$MUSIC"/*.wav "$MUSIC"/*.WAV; do
    [ -f "$w" ] || continue
    mcopy -i "$FAT" "$w" ::/music/
    n=$((n + 1))
done
echo "   $n canciones en music/"

echo ">> Imagen y tabla de particiones"
rm -f "$IMG"
truncate -s "$((FAT_START_MIB + FAT_MIB))M" "$IMG"
sfdisk -q "$IMG" <<EOF
label: dos
start=$((FAT_START_MIB * 2048)), size=$((FAT_MIB * 2048)), type=c, bootable
start=$((A2_START_MIB * 2048)), size=$((A2_MIB * 2048)), type=a2
EOF
dd if="$FAT" of="$IMG" bs=1M seek="$FAT_START_MIB" conv=notrunc status=none
dd if="$OUT/u-boot-with-spl.sfp" of="$IMG" bs=1M seek="$A2_START_MIB" conv=notrunc status=none
rm -f "$FAT"
sfdisk -l "$IMG" | sed 's/^/   /'
echo ">> Listo: $IMG"
