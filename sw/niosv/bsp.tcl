# Ajustes del BSP del Nios V (NV-01). scripts/build-niosv.sh crea el BSP con
# --no-default y este script, por dos razones:
#
#  1. Restriccion del enunciado: el software no usa el HAL salvo para
#     interrupciones, asi que los IP del University Program quedan SIN driver HAL;
#     nuestros drivers (sw/niosv/drivers/) acceden a sus registros por puntero.
#  2. El driver HAL del IP de Audio no soporta la API de interrupciones del Nios V
#     e invalida el BSP al final del script por defecto, antes de que se pueda
#     quitar. Por eso aqui se replica lo que ese script hacia (stdio, timer y
#     secciones del enlazador), tomado de niosv/scripts/bsp-defaults/.

# ---- Drivers: ninguno para los IP del University Program ----
foreach inst {audio av_config adc char_buf video_fifo vga audio_pll video_pll} {
    set_driver none $inst
}

# ---- stdio por JTAG UART (solo depuracion y mediciones) y timer del sistema ----
set_setting hal.stdin  none
set_setting hal.stdout juart
set_setting hal.stderr juart
set_setting hal.sys_clk_timer cpu
set_setting hal.timestamp_timer cpu

# ---- Memoria (CD-06): todo el programa en la RAM on-chip de 128 KB ----
# add_default_memory_regions crea una region por memoria del SoC (y la de reset).
# shm no se usa como region: se accede solo por puntero (protocolo SC-13).
add_default_memory_regions
foreach sec {.entry .text .exceptions .rodata .rwdata .bss .heap .stack} {
    add_section_mapping $sec ram
}
set_setting hal.linker.exception_stack_memory_region_name ram
# El programa vive en la RAM que inicializa ram.hex: no hay bootloader ni
# copias de secciones al arrancar (arranque mas rapido, §3-F p.4).
set_setting hal.linker.allow_code_at_reset true
set_setting hal.linker.enable_alt_load false
set_setting hal.linker.enable_alt_load_copy_rodata false
set_setting hal.linker.enable_alt_load_copy_rwdata false
set_setting hal.linker.enable_alt_load_copy_exceptions false

# ---- Codigo mas pequeno ----
set_setting hal.enable_c_plus_plus false
set_setting hal.enable_reduced_device_drivers true
# picolibc en vez de newlib: su printf es mucho mas pequeno (la prueba de humo
# baja de ~87 KB a unos pocos KB de .text). Sin exit(): el reproductor no termina.
set_setting hal.linker.use_picolibc true
set_setting hal.enable_exit false
set_setting hal.enable_clean_exit false

# Sin -fstack-protector-strong: agrega un chequeo en cada funcion con arreglos locales
# (ciclos en el lazo de audio) y la RAM es on-chip sin otros procesos que la corrompan.
set_setting hal.make.enable_cflag_fstack_protector_strong false
