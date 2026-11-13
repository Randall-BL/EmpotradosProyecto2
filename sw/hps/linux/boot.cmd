# Script de arranque de U-Boot (HP-03, HP-04). Se compila a boot.scr con mkimage y va
# en la particion FAT de la SD, donde lo encuentra el arranque estandar de U-Boot.
echo "== Reproductor de audio: arranque del HPS =="

# HP-04: si la SD trae el bitstream, U-Boot configura la FPGA antes de Linux. Sin el
# archivo la FPGA se programa por JTAG (desarrollo) y la aplicacion la espera.
# El remap del L3 (0xFF80_0000 = 0x19, puentes visibles) se escribe aqui y no desde
# Linux: con el kernel corriendo, escribirlo congela el sistema (probado el 12 nov).
if load mmc 0:1 0x03000000 audio_player.rbf; then
    fpga load 0 0x03000000 ${filesize}
    bridge enable
else
    mw.l 0xff800000 0x19
fi

load mmc 0:1 0x01000000 zImage
load mmc 0:1 0x02000000 de1_soc_audio.dtb
setenv bootargs console=ttyS0,115200 quiet
bootz 0x01000000 - 0x02000000
