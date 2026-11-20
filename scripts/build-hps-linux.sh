#!/usr/bin/env bash
# Construye el arranque del HPS (HP-02): preloader + U-Boot, kernel Linux con el rootfs
# minimo en initramfs (BusyBox + hps_player) y el device tree de la DE1-SoC.
#
# Corre dentro del contenedor docker/hps.Dockerfile:
#   docker compose -f docker/compose.yml run --rm hps scripts/build-hps-linux.sh
#
# Las fuentes se descargan una vez a build/hps/src (con su SHA-256 verificado) y se
# compilan en build/hps/. Salida en build/hps/out/:
#   u-boot-with-spl.sfp   preloader (SPL) + U-Boot, va crudo en la particion A2
#   zImage                kernel con el initramfs dentro
#   de1_soc_audio.dtb     device tree
#   boot.scr              script de arranque de U-Boot (sw/hps/linux/boot.cmd)
#
# U-Boot usa socfpga_de1_soc_defconfig: su configuracion de la DDR3 y de los pines del
# HPS (board/terasic/de1-soc/qts) es la del GHRD de Terasic, la misma que se puso al HPS
# en hw/soc/build_system.tcl (HW-05).
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
B="$HERE/build/hps"
SRC="$B/src"
OUT="$B/out"
LNX="$HERE/sw/hps/linux"
CROSS=arm-linux-gnueabihf-
JOBS="${JOBS:-$(nproc)}"
mkdir -p "$SRC" "$OUT"

UBOOT_VER=2024.01
LINUX_VER=6.6.60
BUSYBOX_VER=1.36.1
# Espejo oficial de U-Boot en GitHub (ftp.denx.de no respondia el 12 nov 2026).
UBOOT_URL="https://github.com/u-boot/u-boot/archive/refs/tags/v$UBOOT_VER.tar.gz"
LINUX_URL="https://cdn.kernel.org/pub/linux/kernel/v6.x/linux-$LINUX_VER.tar.xz"
BUSYBOX_URL="https://busybox.net/downloads/busybox-$BUSYBOX_VER.tar.bz2"
SUMS="$LNX/sources.sha256"

fetch() {                                  # fetch <url> -> descomprime en $SRC
    local url="$1" file="$SRC/$(basename "$1")"
    if [ ! -f "$file" ]; then
        echo ">> Descargando $url"
        curl -fsSL --retry 3 --connect-timeout 20 -o "$file.part" "$url"
        mv "$file.part" "$file"
    fi
    # Verifica contra sources.sha256; la primera vez registra el hash (TOFU).
    local name sum
    name="$(basename "$file")"
    sum="$(sha256sum "$file" | cut -d' ' -f1)"
    if grep -q " $name\$" "$SUMS" 2>/dev/null; then
        (cd "$SRC" && grep " $name\$" "$SUMS" | sha256sum -c --quiet -) \
            || { echo "ERROR: SHA-256 de $name no coincide" >&2; exit 1; }
    else
        echo "$sum  $name" >> "$SUMS"
        echo "   registrado SHA-256 de $name en sw/hps/linux/sources.sha256"
    fi
    [ -f "$file.extracted" ] || { tar -xf "$file" -C "$SRC" && touch "$file.extracted"; }
}

cd "$SRC"
fetch "$UBOOT_URL"
fetch "$LINUX_URL"
fetch "$BUSYBOX_URL"

# Parches propios del kernel (sw/hps/linux/patches), una sola vez por arbol extraido.
for p in "$LNX"/patches/*.patch; do
    [ -e "$p" ] || continue
    stamp="$SRC/linux-$LINUX_VER/.applied-$(basename "$p")"
    [ -f "$stamp" ] && continue
    echo ">> Parche del kernel: $(basename "$p")"
    patch -d "$SRC/linux-$LINUX_VER" -p1 --forward --quiet < "$p"
    touch "$stamp"
done

# ---------------------------------------------------------------- U-Boot
echo ">> U-Boot $UBOOT_VER (socfpga_de1_soc_defconfig)"
UB="$SRC/u-boot-$UBOOT_VER"
make -C "$UB" CROSS_COMPILE=$CROSS O="$B/u-boot" socfpga_de1_soc_defconfig >/dev/null
# El defconfig no define bootcmd (probado en la tarjeta el 12 nov: U-Boot se quedaba en
# el prompt). Se arranca con el distro boot, que encuentra boot.scr en la FAT; 1 s de
# espera para poder interrumpir desde la consola sin alargar el arranque (< 10 s).
"$UB/scripts/config" --file "$B/u-boot/.config" \
    --enable USE_BOOTCOMMAND --set-str BOOTCOMMAND "run distro_bootcmd" \
    --set-val BOOTDELAY 1
make -C "$UB" CROSS_COMPILE=$CROSS O="$B/u-boot" olddefconfig >/dev/null
make -C "$UB" CROSS_COMPILE=$CROSS O="$B/u-boot" -j"$JOBS" > "$B/u-boot.log" 2>&1 \
    || { tail -30 "$B/u-boot.log"; exit 1; }
cp "$B/u-boot/u-boot-with-spl.sfp" "$OUT/"

# ---------------------------------------------------------------- aplicacion
echo ">> hps_player"
make -C "$HERE/sw/hps" ARM_CC=${CROSS}gcc > "$B/hps_player.log" 2>&1 \
    || { cat "$B/hps_player.log"; exit 1; }

# ---------------------------------------------------------------- BusyBox
echo ">> BusyBox $BUSYBOX_VER (estatico)"
BB="$SRC/busybox-$BUSYBOX_VER"
mkdir -p "$B/busybox"
if [ ! -f "$B/busybox/.config" ]; then
    make -C "$BB" O="$B/busybox" ARCH=arm CROSS_COMPILE=$CROSS defconfig >/dev/null
    # Estatico (el initramfs no lleva libc compartida); "tc" no compila con los
    # headers de kernels nuevos y no hace falta.
    sed -i -e 's/^# CONFIG_STATIC is not set/CONFIG_STATIC=y/' \
           -e 's/^CONFIG_TC=y/# CONFIG_TC is not set/' "$B/busybox/.config"
    make -C "$BB" O="$B/busybox" ARCH=arm CROSS_COMPILE=$CROSS oldconfig </dev/null >/dev/null
fi
make -C "$B/busybox" ARCH=arm CROSS_COMPILE=$CROSS -j"$JOBS" > "$B/busybox.log" 2>&1 \
    || { tail -30 "$B/busybox.log"; exit 1; }

# ---------------------------------------------------------------- initramfs
# Lista para gen_init_cpio del kernel: no hace falta root para crear /dev/console.
cat > "$B/initramfs.list" <<EOF
dir /dev 0755 0 0
nod /dev/console 0600 0 0 c 5 1
dir /proc 0755 0 0
dir /sys 0755 0 0
dir /mnt 0755 0 0
dir /mnt/sd 0755 0 0
dir /bin 0755 0 0
dir /sbin 0755 0 0
dir /usr 0755 0 0
dir /usr/bin 0755 0 0
file /bin/busybox $B/busybox/busybox 0755 0 0
slink /bin/sh busybox 0777 0 0
slink /bin/mount busybox 0777 0 0
slink /bin/sleep busybox 0777 0 0
slink /bin/echo busybox 0777 0 0
slink /bin/cat busybox 0777 0 0
slink /bin/ls busybox 0777 0 0
slink /bin/dmesg busybox 0777 0 0
slink /sbin/devmem ../bin/busybox 0777 0 0
file /usr/bin/hps_player $B/hps_player 0755 0 0
file /init $LNX/init 0755 0 0
EOF

# ---------------------------------------------------------------- kernel
echo ">> Linux $LINUX_VER (socfpga_defconfig + sw/hps/linux/kernel.fragment)"
KS="$SRC/linux-$LINUX_VER"
KB="$B/linux"
mkdir -p "$KB"
make -C "$KS" O="$KB" ARCH=arm CROSS_COMPILE=$CROSS socfpga_defconfig >/dev/null
sed "s#/work/build/hps#$B#" "$LNX/kernel.fragment" > "$KB/fragment.config"
(cd "$KB" && "$KS/scripts/kconfig/merge_config.sh" -m .config fragment.config >/dev/null)
make -C "$KS" O="$KB" ARCH=arm CROSS_COMPILE=$CROSS olddefconfig >/dev/null
make -C "$KS" O="$KB" ARCH=arm CROSS_COMPILE=$CROSS -j"$JOBS" zImage > "$B/linux.log" 2>&1 \
    || { tail -30 "$B/linux.log"; exit 1; }
cp "$KB/arch/arm/boot/zImage" "$OUT/"

# Device tree propio: se preprocesa con los includes del kernel y se compila con su dtc.
DTS_DIR="$KS/arch/arm/boot/dts/intel/socfpga"
make -C "$KS" O="$KB" ARCH=arm CROSS_COMPILE=$CROSS scripts_dtc >/dev/null
cpp -nostdinc -undef -D__DTS__ -x assembler-with-cpp -I "$DTS_DIR" -I "$KS/include" \
    "$LNX/de1_soc_audio.dts" -o "$B/de1_soc_audio.dts.pp"
"$KB/scripts/dtc/dtc" -I dts -O dtb -q -o "$OUT/de1_soc_audio.dtb" "$B/de1_soc_audio.dts.pp"

# ---------------------------------------------------------------- boot.scr
mkimage -A arm -T script -C none -n "reproductor" -d "$LNX/boot.cmd" "$OUT/boot.scr" >/dev/null

echo ">> Listo en build/hps/out:"
ls -la "$OUT"
