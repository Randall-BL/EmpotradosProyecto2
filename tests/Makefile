# Pruebas en el PC de la logica portable (sw/common/), DK-14.   Uso: make -C tests
#
# Mismas banderas que exige CONTRIBUTING.md, con -Werror y sanitizers (ASan + UBSan):
# una lectura fuera de un buffer o un desborde con signo hace fallar la prueba.
# Sin sanitizers (p. ej. otro compilador):  make -C tests SAN=
# Los WAV de prueba se generan en build/tests/data con tests/data/gen_wavs.py.
CC      ?= gcc
PYTHON  ?= python3
SAN     ?= -fsanitize=address,undefined -fno-sanitize-recover=all
CFLAGS  ?= -std=c11 -Wall -Wextra -Werror -O2 -g
CFLAGS  += $(SAN) -I../sw/common/include
LDFLAGS += $(SAN)
BUILD   := ../build/tests
DATA    := $(BUILD)/data
C       := ../sw/common/src
HDRS    := $(wildcard ../sw/common/include/*.h) test.h

TESTS   := test_volume test_screen test_tone test_dsp test_wav test_fsm test_input

# Fuentes de sw/common que usa cada prueba.
SRC_test_volume := volume.c
SRC_test_screen := screen.c
SRC_test_tone   := tone.c
SRC_test_dsp    := dsp.c
SRC_test_wav    := wav.c le.c err.c
SRC_test_fsm    := fsm.c playlist.c
SRC_test_input  := evq.c switches.c debounce.c clock.c
SRC_test_shm    := shm_proto.c
SRC_test_app    := app.c fsm.c playlist.c evq.c switches.c debounce.c shm_proto.c \
                   shm_source.c hps_model.c screen.c volume.c dsp.c err.c
LIBS_test_tone  := -lm
DEFS_test_tone  := -D_DEFAULT_SOURCE

.PHONY: all build clean
all: build $(DATA)/.stamp
	@set -e; for t in $(TESTS); do \
	    echo "== $$t"; \
	    $(BUILD)/$$t $(abspath $(DATA)); \
	done
# Las pruebas leen ../sim/vectors: se corren desde tests/ (make -C tests).

build: $(addprefix $(BUILD)/,$(TESTS))

.SECONDEXPANSION:
$(BUILD)/%: %.c $$(addprefix $(C)/,$$(SRC_$$*)) $(HDRS) | $(BUILD)
	$(CC) $(CFLAGS) $(DEFS_$*) -o $@ $< $(addprefix $(C)/,$(SRC_$*)) $(LDFLAGS) $(LIBS_$*)

$(DATA)/.stamp: data/gen_wavs.py | $(BUILD)
	$(PYTHON) data/gen_wavs.py $(DATA)
	@touch $@

$(BUILD):
	mkdir -p $@

clean:
	rm -rf ../build/tests
