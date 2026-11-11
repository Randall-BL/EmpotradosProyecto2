# Makefile raiz (DK-10): lo que corre dentro del contenedor y en la CI.
#
#   make test           pruebas en C de sw/common (tests/, con sanitizers)
#   make sim            pruebas cocotb + Icarus de cada modulo HDL propio (sim/cocotb/*)
#   make lint           Verilator --lint-only del HDL propio + auditoria sin HAL (NV-17)
#   make hps            software del HPS (sw/hps) o, mientras no exista, sw/common
#                       compilado para ARM como prueba de portabilidad
#   make ci             lo mismo que corre la CI: lint, test, sim y hps
#   make clean          borra lo que generan estos objetivos (build/tests, build/sim/cocotb...)
#
#   make docker-build   construye la imagen (docker/compose.yml)
#   make docker-shell   abre una terminal en el contenedor con el repo en /work
#   make docker-<obj>   corre "make <obj>" dentro del contenedor, p. ej. make docker-ci
#
# Quartus, Platform Designer, el BSP del Nios V y Questa NO estan aqui: se usan de forma
# nativa con scripts/ (docs/entorno-nativo.md). scripts/test-all.sh corre las pruebas
# del SoC en Questa.

SHELL      := /bin/bash
BUILD      := build
RTL        := hw/rtl
RTL_TOPS   := filtro_audio seg7_time
COCOTB_DIRS := $(sort $(dir $(wildcard sim/cocotb/*/Makefile)))
ARM_CC     ?= arm-linux-gnueabihf-gcc
COMPOSE    := docker compose -f docker/compose.yml
export HOST_UID := $(shell id -u)
export HOST_GID := $(shell id -g)

.PHONY: all ci test sim lint lint-hdl audit hps clean help docker-build docker-shell

all: ci
ci: lint test sim hps

help:
	@sed -n '2,19p' Makefile

# ---------------------------------------------------------------- pruebas en C
test:
	$(MAKE) -C tests

# ---------------------------------------------------------------- cocotb (DK-16)
# Cada carpeta sim/cocotb/<modulo>/ trae un Makefile de cocotb. Los resultados JUnit
# y las formas de onda quedan en build/sim/cocotb/<modulo>/.
sim:
	@if [ -z "$(COCOTB_DIRS)" ]; then \
	    echo "make sim: todavia no hay pruebas cocotb en sim/cocotb/ (DK-09, DK-11..13)"; \
	    echo "          el HDL propio se verifica en Questa: scripts/sim-rtl.sh"; \
	    exit 0; \
	fi; \
	set -e; for d in $(COCOTB_DIRS); do \
	    n=$$(basename $$d); out=$(CURDIR)/$(BUILD)/sim/cocotb/$$n; mkdir -p $$out; \
	    echo "== cocotb $$n"; \
	    $(MAKE) -C $$d SIM=icarus WAVES=1 SIM_BUILD=$$out \
	        COCOTB_RESULTS_FILE=$$out/results.xml; \
	    if grep -q '<failure' $$out/results.xml; then echo "FALLO en $$n"; exit 1; fi; \
	done

# ---------------------------------------------------------------- lint (NV-17)
lint: lint-hdl audit

lint-hdl:
	@set -e; for t in $(RTL_TOPS); do \
	    echo "== verilator --lint-only -Wall $$t"; \
	    verilator --lint-only -Wall -I$(RTL) $(RTL)/lint.vlt --top-module $$t $(RTL)/*.v; \
	done

audit:
	scripts/audit-hal.sh

# ---------------------------------------------------------------- HPS
# Cuando D agregue sw/hps/Makefile (HP-01, DK-15) se usa ese. Mientras tanto se compila
# sw/common para el ARM del HPS: el parser WAV y el protocolo los usa el HPS (HP-06/07).
hps:
	@if [ -f sw/hps/Makefile ]; then \
	    $(MAKE) -C sw/hps; \
	else \
	    set -e; mkdir -p $(BUILD)/hps/common; \
	    for f in sw/common/src/*.c; do \
	        $(ARM_CC) -std=c11 -Wall -Wextra -Werror -O2 -Isw/common/include \
	            -c $$f -o $(BUILD)/hps/common/$$(basename $${f%.c}).o; \
	    done; \
	    echo "make hps: sw/common compila para ARM ($(ARM_CC)) en $(BUILD)/hps/common"; \
	    file $(BUILD)/hps/common/wav.o; \
	fi

clean:
	$(MAKE) -C tests clean
	rm -rf $(BUILD)/sim/cocotb $(BUILD)/hps/common

# ---------------------------------------------------------------- contenedor
docker-build:
	$(COMPOSE) build

docker-shell:
	$(COMPOSE) run --rm dev bash

docker-%:
	$(COMPOSE) run --rm dev make $*
