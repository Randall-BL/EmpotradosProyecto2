# SoC del Proyecto II (reproductor de audio) en Platform Designer.
#
# Origen: Taller 9, hw/build_system.tcl (ver docs/reutilizado-taller9.md).
# Es la unica fuente del SoC: el .qsys se regenera desde aqui (HW-01).
# Decisiones que fijan los parametros: docs/decisiones/CD-06.md (memoria),
# CD-07.md (audio a 48 kHz) y CD-09.md (VGA). Mapa de memoria: docs/hw/mapa-memoria.md.
#
# Regenerar el .qsys:  cd hw/soc && qsys-script --script=build_system.tcl
# (scripts/build-soc.sh lo hace solo si falta el .qsys o con REGEN_QSYS=1)

package require -exact qsys 25.1
create_system audio_soc
set_project_property DEVICE_FAMILY {Cyclone V}
set_project_property DEVICE {5CSEMA5F31C6}

# ============================================================================
#  Reloj, procesador y memoria
# ============================================================================

# ---- Reloj del sistema: CLOCK_50 ----
add_instance clk clock_source
set_instance_parameter_value clk clockFrequency 50000000
set_instance_parameter_value clk resetSynchronousEdges {DEASSERT}

# ---- Nios V/m (HW-03): pipeline activo, arranca desde la RAM ----
# El pipeline baja el CPI cerca de 1; sin el, el lazo de audio a 48 kHz usa
# la mayor parte de los 50 MHz (ver docs/decisiones/CD-06.md).
add_instance cpu intel_niosv_m
set_instance_parameter_value cpu pipelineArch 1
set_instance_parameter_value cpu resetSlave {ram.s1}
set_instance_parameter_value cpu resetOffset 0
set_instance_parameter_value cpu enableDebug 1

# ---- RAM on-chip de 128 KB para codigo y datos (CD-06) ----
add_instance ram altera_avalon_onchip_memory2
set_instance_parameter_value ram memorySize 131072
set_instance_parameter_value ram initMemContent 1
set_instance_parameter_value ram useNonDefaultInitFile 1
set_instance_parameter_value ram initializationFileName {ram.hex}
set_instance_parameter_value ram writable 1
set_instance_parameter_value ram dualPort 0

# ---- Memoria compartida HPS <-> Nios V, 64 KB de doble puerto (CD-06, HW-06) ----
# s1: Nios V.  s2: queda para el bridge del HPS (lo conecta HW-05).
# El protocolo (cabecera, tabla de pistas e indices del ring buffer) vive al
# inicio; no hace falta buzon aparte porque los indices son el handshake.
add_instance shm altera_avalon_onchip_memory2
set_instance_parameter_value shm memorySize 65536
set_instance_parameter_value shm dualPort 1
set_instance_parameter_value shm writable 1
set_instance_parameter_value shm initMemContent 0

# ---- JTAG UART: stdout de depuracion y mediciones (se revisa en HW-23) ----
add_instance juart altera_avalon_jtag_uart

# ============================================================================
#  PIO de la tarjeta (HW-02)
# ============================================================================

# ---- KEY[3:0]: botones, activos en bajo -> flanco de bajada con IRQ ----
add_instance pio_key altera_avalon_pio
set_instance_parameter_value pio_key direction {Input}
set_instance_parameter_value pio_key width {4}
set_instance_parameter_value pio_key captureEdge 1
set_instance_parameter_value pio_key edgeType {FALLING}
set_instance_parameter_value pio_key generateIRQ 1
set_instance_parameter_value pio_key irqType {EDGE}
set_instance_parameter_value pio_key bitClearingEdgeCapReg 1

# ---- SW[9:0]: seleccion de filtro ----
# Captura de cualquier flanco con IRQ: CD-08 elige entre sondeo e interrupcion
# solo con la mascara del PIO, sin regenerar el hardware.
add_instance pio_sw altera_avalon_pio
set_instance_parameter_value pio_sw direction {Input}
set_instance_parameter_value pio_sw width {10}
set_instance_parameter_value pio_sw captureEdge 1
set_instance_parameter_value pio_sw edgeType {ANY}
set_instance_parameter_value pio_sw generateIRQ 1
set_instance_parameter_value pio_sw irqType {EDGE}
set_instance_parameter_value pio_sw bitClearingEdgeCapReg 1

# ---- LEDR[9:0]: estado de reproduccion y filtro activo ----
add_instance pio_led altera_avalon_pio
set_instance_parameter_value pio_led direction {Output}
set_instance_parameter_value pio_led width {10}
set_instance_parameter_value pio_led resetValue {0}

# ============================================================================
#  Conexiones de reloj y reset
# ============================================================================
foreach inst {cpu juart pio_key pio_sw pio_led} {
    add_connection clk.clk $inst.clk
    add_connection clk.clk_reset $inst.reset
}
add_connection clk.clk ram.clk1
add_connection clk.clk_reset ram.reset1
add_connection clk.clk shm.clk1
add_connection clk.clk_reset shm.reset1
add_connection clk.clk shm.clk2
add_connection clk.clk_reset shm.reset2

# ============================================================================
#  Avalon-MM y mapa de direcciones (docs/hw/mapa-memoria.md)
# ============================================================================
proc map {master slave base} {
    add_connection $master $slave
    set_connection_parameter_value $master/$slave baseAddress $base
}

map cpu.instruction_manager ram.s1                 0x00000000
map cpu.data_manager        ram.s1                 0x00000000
map cpu.data_manager        shm.s1                 0x00020000
map cpu.data_manager        juart.avalon_jtag_slave 0x00040000
map cpu.data_manager        pio_led.s1             0x00040010
map cpu.data_manager        pio_key.s1             0x00040020
map cpu.data_manager        pio_sw.s1              0x00040030
map cpu.data_manager        cpu.timer_sw_agent     0x00040040
map cpu.data_manager        cpu.dm_agent           0x00050000

# ============================================================================
#  Interrupciones (receptor platform_irq_rx del Nios V)
# ============================================================================
proc irq {slave num} {
    add_connection cpu.platform_irq_rx $slave
    set_connection_parameter_value cpu.platform_irq_rx/$slave irqNumber $num
}
irq juart.irq   1
irq pio_key.irq 2
irq pio_sw.irq  3

# ============================================================================
#  Interfaces exportadas al top-level (hw/quartus/DE1_SoC_top.v)
# ============================================================================
add_interface clk clock sink
set_interface_property clk EXPORT_OF clk.clk_in
add_interface reset reset sink
set_interface_property reset EXPORT_OF clk.clk_in_reset

add_interface key conduit end
set_interface_property key EXPORT_OF pio_key.external_connection
add_interface sw conduit end
set_interface_property sw EXPORT_OF pio_sw.external_connection
add_interface led conduit end
set_interface_property led EXPORT_OF pio_led.external_connection

save_system audio_soc.qsys
