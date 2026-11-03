# Pruebas en el PC de la logica portable (sw/common/).  Uso: make -C tests
# Mismas banderas que exige CONTRIBUTING.md, con -Werror para que no pase nada.
CC      ?= gcc
CFLAGS  ?= -std=c11 -Wall -Wextra -Werror -O2 -I../sw/common/include
BUILD   := ../build/tests
TESTS   := test_volume test_screen test_tone test_dsp

.PHONY: all clean
all: $(addprefix $(BUILD)/,$(TESTS))
	@set -e; for t in $^; do $$t; done
# Las pruebas leen ../sim/vectors: se corren desde tests/ (make -C tests).

$(BUILD)/test_volume: test_volume.c ../sw/common/src/volume.c ../sw/common/include/volume.h | $(BUILD)
	$(CC) $(CFLAGS) -o $@ test_volume.c ../sw/common/src/volume.c

$(BUILD)/test_screen: test_screen.c ../sw/common/src/screen.c ../sw/common/include/screen.h | $(BUILD)
	$(CC) $(CFLAGS) -o $@ test_screen.c ../sw/common/src/screen.c

$(BUILD)/test_tone: test_tone.c ../sw/common/src/tone.c ../sw/common/include/tone.h | $(BUILD)
	$(CC) $(CFLAGS) -D_DEFAULT_SOURCE -o $@ test_tone.c ../sw/common/src/tone.c -lm

$(BUILD)/test_dsp: test_dsp.c ../sw/common/src/dsp.c ../sw/common/include/dsp.h ../sw/common/include/fir_coefs.h | $(BUILD)
	$(CC) $(CFLAGS) -o $@ test_dsp.c ../sw/common/src/dsp.c

$(BUILD):
	mkdir -p $@

clean:
	rm -rf $(BUILD)
