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
#  Audio (HW-07, CD-07): codec WM8731 fijo a 48 kHz, 16 bits, Left Justified
# ============================================================================

# ---- Audio PLL: MCLK del codec (AUD_XCK) = 12.288 MHz = 256 x 48 kHz ----
add_instance audio_pll altera_up_avalon_audio_pll
set_instance_parameter_value audio_pll audio_clk_freq 12.288

# ---- IP de Audio: solo salida (no se graba), FIFO de 128 muestras por canal ----
add_instance audio altera_up_avalon_audio
set_instance_parameter_value audio avalon_bus_type {Memory Mapped}
set_instance_parameter_value audio audio_in 0
set_instance_parameter_value audio audio_out 1
set_instance_parameter_value audio dw 16

# ---- Audio and Video Config: I2C al WM8731 con autoinicializacion ----
add_instance av_config altera_up_avalon_audio_and_video_config
set_instance_parameter_value av_config device {On-Board Peripherals}
set_instance_parameter_value av_config board {DE1-SoC}
set_instance_parameter_value av_config eai 1
set_instance_parameter_value av_config audio_in {Line In to ADC}
set_instance_parameter_value av_config dac_enable 1
set_instance_parameter_value av_config line_in_bypass 0
set_instance_parameter_value av_config mic_bypass 0
set_instance_parameter_value av_config data_format {Left Justified}
set_instance_parameter_value av_config bit_length 16
set_instance_parameter_value av_config sampling_rate {48 kHz}

# ============================================================================
#  ADC del potenciometro de volumen (HW-08)
# ============================================================================
# El IP detecta solo si la placa trae AD7928 o LTC2308 (isLTC en
# altera_up_avalon_de1_soc_adv_adc.v): adc_cs_n va al pin ADC_CONVST.
# Potenciometro en CH0. Se convierten DOS canales, no uno: con el LTC2308 el
# resultado llega una trama tarde y el IP lo guarda en el registro del canal
# anterior (address 0 -> reading7, 1 -> reading0). Con un solo canal, CH0 nunca
# se actualizaria en esa revision de la placa; con dos, CH0 es correcto en ambas
# (verificado con sim/questa/tb_adc.sv para los dos chips).
add_instance adc altera_up_avalon_de1_soc_adc
set_instance_parameter_value adc numch_ 2
set_instance_parameter_value adc sclk_freq 12.5

# ============================================================================
#  Video VGA 640x480 con texto 80x60 (HW-09, CD-09)
# ============================================================================
# char_buf (RGB de 30 bits) -> video_fifo (50 -> 25 MHz) -> vga. El controlador VGA
# de la DE1-SoC usa 10 bits por color, igual que la salida del char_buf: no hace
# falta remuestrear el color.
add_instance video_pll altera_up_avalon_video_pll
set_instance_parameter_value video_pll video_in_clk_en 0
set_instance_parameter_value video_pll vga_clk_en 1
set_instance_parameter_value video_pll lcd_clk_en 0
set_instance_parameter_value video_pll gui_resolution {VGA 640x480}

add_instance char_buf altera_up_avalon_video_character_buffer_with_dma
set_instance_parameter_value char_buf vga_device {On-board VGA DAC}
set_instance_parameter_value char_buf enable_transparency 0
set_instance_parameter_value char_buf resolution {80 x 60}

add_instance video_fifo altera_up_avalon_video_dual_clock_buffer
set_instance_parameter_value video_fifo color_bits 10
set_instance_parameter_value video_fifo color_planes 3

add_instance vga altera_up_avalon_video_vga_controller
set_instance_parameter_value vga board {DE1-SoC}
set_instance_parameter_value vga device {VGA Connector}
set_instance_parameter_value vga resolution {VGA 640x480}

# ============================================================================
#  Hardware propio (B): filtro FIR con ganancia y displays MM:SS (HW-17)
# ============================================================================
# Componentes en hw/rtl/*_hw.tcl (build-soc.sh agrega hw/rtl a la ruta de busqueda).
# Verificados aislados antes de integrarse: docs/hw/verificacion-filtro.md (HW-16).
add_instance filtro filtro_audio
add_instance seg7 seg7_time

# ============================================================================
#  HPS (HW-05, CD-01): ARM Cortex-A9 con Linux minimo (CD-02). Lee la SD y llena
#  el ring de shm por el puente liviano HPS->FPGA (h2f_lw_axi -> shm.s2).
# ============================================================================
# Solo lo que usa el reproductor: DDR3 (memoria de Linux), SD/MMC (canciones y
# arranque), UART0 (consola de arranque para depurar) y el puente liviano. Sin
# puentes FPGA<->HPS de alto ancho de banda ni interrupciones HPS<->FPGA.
# La DDR3 y la temporizacion de la placa son las de la DE1-SoC (GHRD de Terasic,
# las mismas que asume el preloader socfpga_de1_soc de U-Boot).
add_instance hps altera_hps
foreach {param value} {
    HPS_PROTOCOL {DDR3}
    RATE {Full}
    REF_CLK_FREQ {25.0}
    MEM_ASR {Manual}
    MEM_ATCL {Disabled}
    MEM_AUTO_LEVELING_MODE {true}
    MEM_BANKADDR_WIDTH {3}
    MEM_BL {OTF}
    MEM_BT {Sequential}
    MEM_CK_PHASE {0.0}
    MEM_CK_WIDTH {1}
    MEM_CLK_EN_WIDTH {1}
    MEM_CLK_FREQ {400.0}
    MEM_COL_ADDR_WIDTH {10}
    MEM_CS_WIDTH {1}
    MEM_DLL_EN {true}
    MEM_DQ_PER_DQS {8}
    MEM_DQ_WIDTH {32}
    MEM_DRV_STR {RZQ/7}
    MEM_FORMAT {DISCRETE}
    MEM_IF_DM_PINS_EN {true}
    MEM_IF_DQSN_EN {true}
    MEM_INIT_EN {false}
    MEM_MIRROR_ADDRESSING {0}
    MEM_NUMBER_OF_DIMMS {1}
    MEM_NUMBER_OF_RANKS_PER_DEVICE {1}
    MEM_NUMBER_OF_RANKS_PER_DIMM {1}
    MEM_PD {DLL off}
    MEM_RANK_MULTIPLICATION_FACTOR {1}
    MEM_ROW_ADDR_WIDTH {15}
    MEM_RTT_NOM {RZQ/4}
    MEM_RTT_WR {RZQ/4}
    MEM_SRT {Normal}
    MEM_TCL {11}
    MEM_TFAW_NS {30.0}
    MEM_TINIT_US {500}
    MEM_TMRD_CK {4}
    MEM_TRAS_NS {35.0}
    MEM_TRCD_NS {13.75}
    MEM_TREFI_US {7.8}
    MEM_TRFC_NS {260.0}
    MEM_TRP_NS {13.75}
    MEM_TRRD_NS {7.5}
    MEM_TRTP_NS {7.5}
    MEM_TWR_NS {15.0}
    MEM_TWTR {4}
    MEM_USER_LEVELING_MODE {Leveling}
    MEM_VENDOR {Other}
    MEM_VOLTAGE {1.5V DDR3}
    MEM_WTCL {8}
    TIMING_BOARD_AC_EYE_REDUCTION_H {0.0}
    TIMING_BOARD_AC_EYE_REDUCTION_SU {0.0}
    TIMING_BOARD_AC_SKEW {0.02}
    TIMING_BOARD_AC_SLEW_RATE {1.0}
    TIMING_BOARD_AC_TO_CK_SKEW {0.01}
    TIMING_BOARD_CK_CKN_SLEW_RATE {2.0}
    TIMING_BOARD_DELTA_DQS_ARRIVAL_TIME {0.0}
    TIMING_BOARD_DELTA_READ_DQS_ARRIVAL_TIME {0.0}
    TIMING_BOARD_DERATE_METHOD {AUTO}
    TIMING_BOARD_DQS_DQSN_SLEW_RATE {2.0}
    TIMING_BOARD_DQ_EYE_REDUCTION {0.0}
    TIMING_BOARD_DQ_SLEW_RATE {1.0}
    TIMING_BOARD_DQ_TO_DQS_SKEW {0.05}
    TIMING_BOARD_ISI_METHOD {AUTO}
    TIMING_BOARD_MAX_CK_DELAY {0.03}
    TIMING_BOARD_MAX_DQS_DELAY {0.02}
    TIMING_BOARD_READ_DQ_EYE_REDUCTION {0.0}
    TIMING_BOARD_SKEW_BETWEEN_DIMMS {0.05}
    TIMING_BOARD_SKEW_BETWEEN_DQS {0.06}
    TIMING_BOARD_SKEW_CKDQS_DIMM_MAX {0.12}
    TIMING_BOARD_SKEW_CKDQS_DIMM_MIN {0.06}
    TIMING_BOARD_SKEW_WITHIN_DQS {0.01}
    TIMING_BOARD_TDH {0.0}
    TIMING_BOARD_TDS {0.0}
    TIMING_BOARD_TIH {0.0}
    TIMING_BOARD_TIS {0.0}
} {
    set_instance_parameter_value hps $param $value
}
foreach {param value} {
    F2S_Width {0}
    S2F_Width {0}
    LWH2F_Enable {true}
    F2SDRAM_Type {}
    F2SDRAM_Width {}
    SDIO_PinMuxing {HPS I/O Set 0}
    SDIO_Mode {4-bit Data}
    UART0_PinMuxing {HPS I/O Set 0}
    UART0_Mode {No Flow Control}
} {
    set_instance_parameter_value hps $param $value
}

# ============================================================================
#  Conexiones de reloj y reset
# ============================================================================
foreach inst {audio av_config adc char_buf filtro seg7} {
    add_connection clk.clk $inst.clk
    add_connection clk.clk_reset $inst.reset
}
add_connection clk.clk audio_pll.ref_clk
add_connection clk.clk_reset audio_pll.ref_reset
add_connection clk.clk video_pll.ref_clk
add_connection clk.clk_reset video_pll.ref_reset
add_connection clk.clk video_fifo.clock_stream_in
add_connection clk.clk_reset video_fifo.reset_stream_in
add_connection video_pll.vga_clk video_fifo.clock_stream_out
add_connection video_pll.reset_source video_fifo.reset_stream_out
add_connection video_pll.vga_clk vga.clk
add_connection video_pll.reset_source vga.reset

# ---- Avalon-ST de video ----
add_connection char_buf.avalon_char_source video_fifo.avalon_dc_buffer_sink
add_connection video_fifo.avalon_dc_buffer_source vga.avalon_vga_sink

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
# Puente liviano del HPS a 50 MHz, en el mismo dominio que shm.s2.
add_connection clk.clk hps.h2f_lw_axi_clock

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
map cpu.data_manager        audio.avalon_audio_slave 0x00040080
map cpu.data_manager        av_config.avalon_av_config_slave 0x00040090
map cpu.data_manager        adc.adc_slave          0x000400A0
map cpu.data_manager        char_buf.avalon_char_control_slave 0x000400C0
map cpu.data_manager        char_buf.avalon_char_buffer_slave  0x00042000
map cpu.data_manager        filtro.s1              0x00040100
map cpu.data_manager        seg7.s1                0x00040120
map cpu.data_manager        cpu.dm_agent           0x00050000

# HPS: shm.s2 al inicio del puente liviano (0xFF20_0000 visto desde el ARM).
map hps.h2f_lw_axi_master   shm.s2                 0x00000000

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
# Escritura: la FIFO de salida tiene >= 75 % libre (bit WE); permite rellenarla
# por interrupcion en vez de sondear (NV-06).
irq audio.interrupt 4

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

add_interface audio_clk clock source
set_interface_property audio_clk EXPORT_OF audio_pll.audio_clk
add_interface audio conduit end
set_interface_property audio EXPORT_OF audio.external_interface
add_interface av_config conduit end
set_interface_property av_config EXPORT_OF av_config.external_interface
add_interface adc conduit end
set_interface_property adc EXPORT_OF adc.external_interface
add_interface vga conduit end
set_interface_property vga EXPORT_OF vga.external_interface
add_interface hex conduit end
set_interface_property hex EXPORT_OF seg7.hex

# HPS: DDR3 y pines dedicados del HPS (SD, UART0)
add_interface hps_ddr conduit end
set_interface_property hps_ddr EXPORT_OF hps.memory
add_interface hps_io conduit end
set_interface_property hps_io EXPORT_OF hps.hps_io

save_system audio_soc.qsys
