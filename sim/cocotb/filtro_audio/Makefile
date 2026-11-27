# cocotb + Icarus del modulo propio filtro_audio, aislado y solo por Avalon-MM.
#   test_registros: interfaz de registros (DK-12)
#   test_filtro:    datapath contra el modelo bit a bit de scripts/design_fir.py (DK-11)
ROOT           := $(abspath $(CURDIR)/../../..)
TOPLEVEL_LANG   = verilog
SIM            ?= icarus
VERILOG_SOURCES = $(ROOT)/hw/rtl/filtro_audio.v $(ROOT)/hw/rtl/fir_core.v
TOPLEVEL        = filtro_audio
MODULE          = test_registros,test_filtro
export PYTHONPATH := $(abspath $(CURDIR)/..):$(ROOT)/scripts:$(PYTHONPATH)
export REPO_ROOT  := $(ROOT)
include $(shell cocotb-config --makefiles)/Makefile.sim
