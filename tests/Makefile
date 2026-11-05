# Pruebas en el PC de la logica portable (sw/common/).  Uso: make -C tests
# Mismas banderas que exige CONTRIBUTING.md, con -Werror para que no pase nada.
CC      ?= gcc
CFLAGS  ?= -std=c11 -Wall -Wextra -Werror -O2 -I../sw/common
BUILD   := ../build/tests
TESTS   := test_volume test_screen

.PHONY: all clean
all: $(addprefix $(BUILD)/,$(TESTS))
	@set -e; for t in $^; do $$t; done

$(BUILD)/test_volume: test_volume.c ../sw/common/volume.c ../sw/common/volume.h | $(BUILD)
	$(CC) $(CFLAGS) -o $@ test_volume.c ../sw/common/volume.c

$(BUILD)/test_screen: test_screen.c ../sw/common/screen.c ../sw/common/screen.h | $(BUILD)
	$(CC) $(CFLAGS) -o $@ test_screen.c ../sw/common/screen.c

$(BUILD):
	mkdir -p $@

clean:
	rm -rf $(BUILD)
