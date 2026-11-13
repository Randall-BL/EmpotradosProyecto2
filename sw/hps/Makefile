# Aplicacion del HPS (HP-01, DK-15): hps_player para Linux en el Cortex-A9.
#
# Se compila con la toolchain arm-linux-gnueabihf del contenedor (make docker-hps) y se
# enlaza estatica: el rootfs minimo (initramfs) no lleva la libc compartida.
#   make -C sw/hps            ->  build/hps/hps_player
ARM_CC  ?= arm-linux-gnueabihf-gcc
BUILD   := ../../build/hps
COMMON  := ../common
CFLAGS  ?= -std=c11 -Wall -Wextra -Werror -O2
CFLAGS  += -march=armv7-a -mfpu=neon -mfloat-abi=hard -D_DEFAULT_SOURCE \
           -I$(COMMON)/include -Isrc
LDFLAGS += -static

# sw/common que usa el HPS: parser WAV, protocolo de shm y remuestreo a 48 kHz.
COMMON_SRC := wav.c le.c err.c shm_proto.c resample.c
SRC := src/main.c src/producer.c src/devmem.c $(addprefix $(COMMON)/src/,$(COMMON_SRC))
HDRS := $(wildcard src/*.h $(COMMON)/include/*.h)

.PHONY: all clean
all: $(BUILD)/hps_player

$(BUILD)/hps_player: $(SRC) $(HDRS) | $(BUILD)
	$(ARM_CC) $(CFLAGS) -o $@ $(SRC) $(LDFLAGS)
	@file $@

$(BUILD):
	mkdir -p $@

clean:
	rm -f $(BUILD)/hps_player
