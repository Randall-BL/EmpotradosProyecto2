# Aplicacion del HPS (HP-01, DK-15): hps_player para Linux en el Cortex-A9.
#
# Se compila con la toolchain arm-linux-gnueabihf del contenedor (make docker-hps) y se
# enlaza estatica: el rootfs minimo (initramfs) no lleva la libc compartida.
#   make -C sw/hps            ->  build/hps/hps_player
# Compilador cruzado para ARM (Linux, ABI hard-float). Se puede cambiar por variable.
ARM_CC  ?= arm-linux-gnueabihf-gcc
BUILD   := ../../build/hps
COMMON  := ../common
# Opciones: C11 con todas las advertencias como error; codigo para el Cortex-A9
# (ARMv7-A con NEON) y las cabeceras de sw/common.
CFLAGS  ?= -std=c11 -Wall -Wextra -Werror -O2
CFLAGS  += -pthread -march=armv7-a -mfpu=neon -mfloat-abi=hard -D_DEFAULT_SOURCE \
           -I$(COMMON)/include -Isrc
LDFLAGS += -static -pthread

# sw/common que usa el HPS: parser WAV, protocolo de shm y remuestreo a 48 kHz.
COMMON_SRC := wav.c le.c err.c shm_proto.c resample.c
# Fuentes del ejecutable y cabeceras de las que depende (para recompilar si cambian).
SRC := src/main.c src/producer.c src/prefetch.c src/devmem.c $(addprefix $(COMMON)/src/,$(COMMON_SRC))
HDRS := $(wildcard src/*.h $(COMMON)/include/*.h)

.PHONY: all clean
all: $(BUILD)/hps_player

# Compila y enlaza todo en un paso; "file" muestra que el binario es ARM estatico.
$(BUILD)/hps_player: $(SRC) $(HDRS) | $(BUILD)
	$(ARM_CC) $(CFLAGS) -o $@ $(SRC) $(LDFLAGS)
	@file $@

# Carpeta de salida (dependencia de solo orden: no fuerza recompilar).
$(BUILD):
	mkdir -p $@

clean:
	rm -f $(BUILD)/hps_player
