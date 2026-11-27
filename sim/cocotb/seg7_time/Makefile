# cocotb + Icarus del modulo propio seg7_time, aislado y solo por Avalon-MM (DK-13).
ROOT           := $(abspath $(CURDIR)/../../..)
TOPLEVEL_LANG   = verilog
SIM            ?= icarus
VERILOG_SOURCES = $(ROOT)/hw/rtl/seg7_time.v
TOPLEVEL        = seg7_time
MODULE          = test_seg7_time
export PYTHONPATH := $(abspath $(CURDIR)/..):$(PYTHONPATH)
include $(shell cocotb-config --makefiles)/Makefile.sim
