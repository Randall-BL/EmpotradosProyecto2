# cocotb + Icarus: prueba trivial de la cadena de simulacion (DK-09).
TOPLEVEL_LANG   = verilog
SIM            ?= icarus
VERILOG_SOURCES = $(CURDIR)/contador.v
TOPLEVEL        = contador
MODULE          = test_contador
include $(shell cocotb-config --makefiles)/Makefile.sim
