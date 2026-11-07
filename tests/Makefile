# Pruebas en el PC de la logica portable (sw/common/).  Uso: make -C tests
# Mismas banderas que exige CONTRIBUTING.md, con -Werror para que no pase nada.
CC      ?= gcc
CFLAGS  ?= -std=c11 -Wall -Wextra -Werror -O2 -I../sw/common
BUILD   := ../build/tests
TESTS   := test_volume test_screen test_tone test_dsp

.PHONY: all clean
all: $(addprefix $(BUILD)/,$(TESTS))
	@set -e; for t in $^; do $$t; done
# Las pruebas leen ../sim/vectors: se corren desde tests/ (make -C tests).

$(BUILD)/test_volume: test_volume.c ../sw/common/volume.c ../sw/common/volume.h | $(BUILD)
	$(CC) $(CFLAGS) -o $@ test_volume.c ../sw/common/volume.c

$(BUILD)/test_screen: test_screen.c ../sw/common/screen.c ../sw/common/screen.h | $(BUILD)
	$(CC) $(CFLAGS) -o $@ test_screen.c ../sw/common/screen.c

$(BUILD)/test_tone: test_tone.c ../sw/common/tone.c ../sw/common/tone.h | $(BUILD)
	$(CC) $(CFLAGS) -D_DEFAULT_SOURCE -o $@ test_tone.c ../sw/common/tone.c -lm

$(BUILD)/test_dsp: test_dsp.c ../sw/common/dsp.c ../sw/common/dsp.h ../sw/common/fir_coefs.h | $(BUILD)
	$(CC) $(CFLAGS) -o $@ test_dsp.c ../sw/common/dsp.c

$(BUILD):
	mkdir -p $@

clean:
	rm -rf $(BUILD)
