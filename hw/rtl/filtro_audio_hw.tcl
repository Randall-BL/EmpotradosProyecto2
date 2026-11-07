# Componente de Platform Designer del modulo propio filtro_audio (HW-17).
# RTL: filtro_audio.v + fir_core.v. Registros: docs/hw/modulo-filtro.md.
package require -exact qsys 16.1

set_module_property NAME filtro_audio
set_module_property VERSION 1.0
set_module_property DISPLAY_NAME "Filtro FIR de audio (propio)"
set_module_property DESCRIPTION "FIR estereo de 2 bancos con ganancia, esclavo Avalon-MM"
set_module_property GROUP "Proyecto II"
set_module_property AUTHOR "CE-1113 Proyecto II"
set_module_property EDITABLE false

add_fileset QUARTUS_SYNTH QUARTUS_SYNTH "" ""
set_fileset_property QUARTUS_SYNTH TOP_LEVEL filtro_audio
add_fileset_file filtro_audio.v VERILOG PATH filtro_audio.v TOP_LEVEL_FILE
add_fileset_file fir_core.v     VERILOG PATH fir_core.v

add_fileset SIM_VERILOG SIM_VERILOG "" ""
set_fileset_property SIM_VERILOG TOP_LEVEL filtro_audio
add_fileset_file filtro_audio.v VERILOG PATH filtro_audio.v
add_fileset_file fir_core.v     VERILOG PATH fir_core.v

# Sin archivo de coeficientes: el driver (NV-10) los carga al arrancar desde
# sw/common/fir_coefs.h, asi el .hex no tiene que viajar con el IP generado.
add_parameter COEF_FILE STRING ""
set_parameter_property COEF_FILE HDL_PARAMETER true
set_parameter_property COEF_FILE VISIBLE false

add_interface clk clock end
set_interface_property clk clockRate 0
add_interface_port clk clk clk Input 1

add_interface reset reset end
set_interface_property reset associatedClock clk
set_interface_property reset synchronousEdges DEASSERT
add_interface_port reset reset reset Input 1

add_interface s1 avalon end
set_interface_property s1 addressUnits WORDS
set_interface_property s1 associatedClock clk
set_interface_property s1 associatedReset reset
set_interface_property s1 bitsPerSymbol 8
set_interface_property s1 maximumPendingReadTransactions 0
set_interface_property s1 readLatency 0
set_interface_property s1 readWaitTime 0
set_interface_property s1 writeWaitTime 0
set_interface_property s1 explicitAddressSpan 32
add_interface_port s1 avs_address     address     Input  3
add_interface_port s1 avs_read        read        Input  1
add_interface_port s1 avs_write       write       Input  1
add_interface_port s1 avs_writedata   writedata   Input  32
add_interface_port s1 avs_readdata    readdata    Output 32
add_interface_port s1 avs_waitrequest waitrequest Output 1
