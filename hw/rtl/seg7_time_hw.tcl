# Componente de Platform Designer de seg7_time (HW-10, HW-17): tiempo MM:SS y
# numero de cancion en los displays de 7 segmentos.
# Version de la API de componentes de Platform Designer que usa este archivo.
package require -exact qsys 16.1

# Identidad del componente tal como aparece en el catalogo de Platform Designer.
set_module_property NAME seg7_time
set_module_property VERSION 1.0
set_module_property DISPLAY_NAME "Displays MM:SS (propio)"
set_module_property GROUP "Proyecto II"
set_module_property AUTHOR "CE-1113 Proyecto II"
set_module_property EDITABLE false

# Archivos HDL para sintesis (QUARTUS_SYNTH) y para simulacion (SIM_VERILOG).
add_fileset QUARTUS_SYNTH QUARTUS_SYNTH "" ""
set_fileset_property QUARTUS_SYNTH TOP_LEVEL seg7_time
add_fileset_file seg7_time.v VERILOG PATH seg7_time.v TOP_LEVEL_FILE
add_fileset SIM_VERILOG SIM_VERILOG "" ""
set_fileset_property SIM_VERILOG TOP_LEVEL seg7_time
add_fileset_file seg7_time.v VERILOG PATH seg7_time.v

# Reloj y reset de entrada (el reset se suelta de forma sincrona con clk).
add_interface clk clock end
add_interface_port clk clk clk Input 1
add_interface reset reset end
set_interface_property reset associatedClock clk
add_interface_port reset reset reset Input 1

# Esclavo Avalon-MM "s1": direcciones en palabras de 32 bits.
add_interface s1 avalon end
set_interface_property s1 addressUnits WORDS
set_interface_property s1 associatedClock clk
set_interface_property s1 associatedReset reset
# Lectura con latencia fija de 1 ciclo (sin waitrequest).
set_interface_property s1 readLatency 1
set_interface_property s1 explicitAddressSpan 16
add_interface_port s1 avs_address   address   Input  2
add_interface_port s1 avs_read      read      Input  1
add_interface_port s1 avs_write     write     Input  1
add_interface_port s1 avs_writedata writedata Input  32
add_interface_port s1 avs_readdata  readdata  Output 32

# Conducto hacia los pines HEX0..HEX5 (se exporta al nivel superior).
add_interface hex conduit end
set_interface_property hex associatedClock clk
add_interface_port hex hex0 hex0 Output 7
add_interface_port hex hex1 hex1 Output 7
add_interface_port hex hex2 hex2 Output 7
add_interface_port hex hex3 hex3 Output 7
add_interface_port hex hex4 hex4 Output 7
add_interface_port hex hex5 hex5 Output 7
