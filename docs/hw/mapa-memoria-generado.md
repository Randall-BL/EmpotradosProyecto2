# Mapa de memoria exportado de Platform Designer

Generado por `scripts/report-memory-map.py` desde `hw/soc/audio_soc.sopcinfo` (tamanos de `system.h` del BSP).

## Visto desde `cpu.data_manager`

| Base | Fin | Tamaño | Esclavo | Componente |
|---|---|---|---|---|
| `0x00000000` | `0x0001FFFF` | 128 KB | `ram.s1` | altera_avalon_onchip_memory2 |
| `0x00020000` | `0x0002FFFF` | 64 KB | `shm.s1` | altera_avalon_onchip_memory2 |
| `0x00040000` | `0x00040007` | 8 B | `juart.avalon_jtag_slave` | altera_avalon_jtag_uart |
| `0x00040010` | `0x0004001F` | 16 B | `pio_led.s1` | altera_avalon_pio |
| `0x00040020` | `0x0004002F` | 16 B | `pio_key.s1` | altera_avalon_pio |
| `0x00040030` | `0x0004003F` | 16 B | `pio_sw.s1` | altera_avalon_pio |
| `0x00040040` | `0x0004007F` | 64 B | `cpu.timer_sw_agent` | intel_niosv_m |
| `0x00040080` | `0x0004008F` | 16 B | `audio.avalon_audio_slave` | altera_up_avalon_audio |
| `0x00040090` | `0x0004009F` | 16 B | `av_config.avalon_av_config_slave` | altera_up_avalon_audio_and_video_config |
| `0x000400A0` | `0x000400BF` | 32 B | `adc.adc_slave` | altera_up_avalon_de1_soc_adc |
| `0x000400C0` | `0x000400C7` | 8 B | `char_buf.avalon_char_control_slave` | altera_up_avalon_video_character_buffer_with_dma |
| `0x00040100` | `0x0004011F` | 32 B | `filtro.s1` | filtro_audio |
| `0x00040120` | `0x0004012F` | 16 B | `seg7.s1` | seg7_time |
| `0x00042000` | `0x00043FFF` | 8 KB | `char_buf.avalon_char_buffer_slave` | altera_up_avalon_video_character_buffer_with_dma |
| `0x00050000` | `0x0005FFFF` | 64 KB | `cpu.dm_agent` | intel_niosv_m |

## Visto desde `cpu.instruction_manager`

| Base | Fin | Tamaño | Esclavo | Componente |
|---|---|---|---|---|
| `0x00000000` | `0x0001FFFF` | 128 KB | `ram.s1` | altera_avalon_onchip_memory2 |

## Visto desde `hps_arm_a9_0.altera_axi_master`

| Base | Fin | Tamaño | Esclavo | Componente |
|---|---|---|---|---|
| `0x00000000` | `0x7FFFFFFF` | 2097152 KB | `hps_axi_sdram.axi_slave0` | axi_sdram |
| `0xFF200000` | `0xFF3FFFFF` | 2048 KB | `hps_bridges.axi_h2f_lw` | hps_bridge_avalon |
| `0xFF700000` | `0xFF701FFF` | 8 KB | `hps_gmac0.axi_slave0` | stmmac |
| `0xFF702000` | `0xFF703FFF` | 8 KB | `hps_gmac1.axi_slave0` | stmmac |
| `0xFF704000` | `0xFF704FFF` | 4 KB | `hps_sdmmc.axi_slave0` | sdmmc |
| `0xFF705000` | `0xFF7050FF` | 256 B | `hps_qspi.axi_slave0` | cadence_qspi |
| `0xFF706000` | `0xFF706FFF` | 4 KB | `hps_fpgamgr.axi_slave0` | altera_fpgamgr |
| `0xFF708000` | `0xFF7080FF` | 256 B | `hps_gpio0.axi_slave0` | dw_gpio |
| `0xFF709000` | `0xFF7090FF` | 256 B | `hps_gpio1.axi_slave0` | dw_gpio |
| `0xFF70A000` | `0xFF70A0FF` | 256 B | `hps_gpio2.axi_slave0` | dw_gpio |
| `0xFF800000` | `0xFF800FFF` | 4 KB | `hps_l3regs.axi_slave0` | altera_l3regs |
| `0xFF900000` | `0xFF90FFFF` | 64 KB | `hps_nand0.axi_slave0` | denali_nand |
| `0xFFA00000` | `0xFFA000FF` | 256 B | `hps_qspi.axi_slave1` | cadence_qspi |
| `0xFFB00000` | `0xFFB3FFFF` | 256 KB | `hps_usb0.axi_slave0` | usb |
| `0xFFB40000` | `0xFFB7FFFF` | 256 KB | `hps_usb1.axi_slave0` | usb |
| `0xFFB80000` | `0xFFB8FFFF` | 64 KB | `hps_nand0.axi_slave1` | denali_nand |
| `0xFFB90000` | `0xFFB900FF` | 256 B | `hps_fpgamgr.axi_slave1` | altera_fpgamgr |
| `0xFFC00000` | `0xFFC00FFF` | 4 KB | `hps_dcan0.axi_slave0` | bosch_dcan |
| `0xFFC01000` | `0xFFC01FFF` | 4 KB | `hps_dcan1.axi_slave0` | bosch_dcan |
| `0xFFC02000` | `0xFFC020FF` | 256 B | `hps_uart0.axi_slave0` | snps_uart |
| `0xFFC03000` | `0xFFC030FF` | 256 B | `hps_uart1.axi_slave0` | snps_uart |
| `0xFFC04000` | `0xFFC040FF` | 256 B | `hps_i2c0.axi_slave0` | designware_i2c |
| `0xFFC05000` | `0xFFC050FF` | 256 B | `hps_i2c1.axi_slave0` | designware_i2c |
| `0xFFC06000` | `0xFFC060FF` | 256 B | `hps_i2c2.axi_slave0` | designware_i2c |
| `0xFFC07000` | `0xFFC070FF` | 256 B | `hps_i2c3.axi_slave0` | designware_i2c |
| `0xFFC08000` | `0xFFC080FF` | 256 B | `hps_timer0.axi_slave0` | dw_apb_timer_sp |
| `0xFFC09000` | `0xFFC090FF` | 256 B | `hps_timer1.axi_slave0` | dw_apb_timer_sp |
| `0xFFC25000` | `0xFFC25FFF` | 4 KB | `hps_sdrctl.axi_slave0` | altera_sdrctl |
| `0xFFD00000` | `0xFFD000FF` | 256 B | `hps_timer2.axi_slave0` | dw_apb_timer_osc |
| `0xFFD01000` | `0xFFD010FF` | 256 B | `hps_timer3.axi_slave0` | dw_apb_timer_osc |
| `0xFFD02000` | `0xFFD020FF` | 256 B | `hps_wd_timer0.axi_slave0` | dw_wd_timer |
| `0xFFD03000` | `0xFFD030FF` | 256 B | `hps_wd_timer1.axi_slave0` | dw_wd_timer |
| `0xFFD04000` | `0xFFD04FFF` | 4 KB | `hps_clkmgr.axi_slave0` | asimov_clkmgr |
| `0xFFD05000` | `0xFFD050FF` | 256 B | `hps_rstmgr.axi_slave0` | altera_rstmgr |
| `0xFFD08000` | `0xFFD083FF` | 1 KB | `hps_sysmgr.axi_slave0` | altera_sysmgr |
| `0xFFE01000` | `0xFFE01FFF` | 4 KB | `hps_dma.axi_slave0` | arm_pl330_dma |
| `0xFFF00000` | `0xFFF000FF` | 256 B | `hps_spim0.axi_slave0` | spi |
| `0xFFF01000` | `0xFFF010FF` | 256 B | `hps_spim1.axi_slave0` | spi |
| `0xFFFEC000` | `0xFFFEC0FF` | 256 B | `hps_scu.axi_slave0` | scu |
| `0xFFFEC100` | `0xFFFEC1FF` | 256 B | `hps_arm_gic_0.axi_slave1` | arm_gic |
| `0xFFFEC600` | `0xFFFEC6FF` | 256 B | `hps_timer.axi_slave0` | arm_internal_timer |
| `0xFFFED000` | `0xFFFEDFFF` | 4 KB | `hps_arm_gic_0.axi_slave0` | arm_gic |
| `0xFFFEF000` | `0xFFFEFFFF` | 4 KB | `hps_L2.axi_slave0` | arm_pl310_L2 |
| `0xFFFF0000` | `0xFFFFFFFF` | 64 KB | `hps_axi_ocram.axi_slave0` | axi_ocram |

## Visto desde `hps_arm_a9_1.altera_axi_master`

| Base | Fin | Tamaño | Esclavo | Componente |
|---|---|---|---|---|
| `0x00000000` | `0x7FFFFFFF` | 2097152 KB | `hps_axi_sdram.axi_slave0` | axi_sdram |
| `0xFF200000` | `0xFF3FFFFF` | 2048 KB | `hps_bridges.axi_h2f_lw` | hps_bridge_avalon |
| `0xFF700000` | `0xFF701FFF` | 8 KB | `hps_gmac0.axi_slave0` | stmmac |
| `0xFF702000` | `0xFF703FFF` | 8 KB | `hps_gmac1.axi_slave0` | stmmac |
| `0xFF704000` | `0xFF704FFF` | 4 KB | `hps_sdmmc.axi_slave0` | sdmmc |
| `0xFF705000` | `0xFF7050FF` | 256 B | `hps_qspi.axi_slave0` | cadence_qspi |
| `0xFF706000` | `0xFF706FFF` | 4 KB | `hps_fpgamgr.axi_slave0` | altera_fpgamgr |
| `0xFF708000` | `0xFF7080FF` | 256 B | `hps_gpio0.axi_slave0` | dw_gpio |
| `0xFF709000` | `0xFF7090FF` | 256 B | `hps_gpio1.axi_slave0` | dw_gpio |
| `0xFF70A000` | `0xFF70A0FF` | 256 B | `hps_gpio2.axi_slave0` | dw_gpio |
| `0xFF800000` | `0xFF800FFF` | 4 KB | `hps_l3regs.axi_slave0` | altera_l3regs |
| `0xFF900000` | `0xFF90FFFF` | 64 KB | `hps_nand0.axi_slave0` | denali_nand |
| `0xFFA00000` | `0xFFA000FF` | 256 B | `hps_qspi.axi_slave1` | cadence_qspi |
| `0xFFB00000` | `0xFFB3FFFF` | 256 KB | `hps_usb0.axi_slave0` | usb |
| `0xFFB40000` | `0xFFB7FFFF` | 256 KB | `hps_usb1.axi_slave0` | usb |
| `0xFFB80000` | `0xFFB8FFFF` | 64 KB | `hps_nand0.axi_slave1` | denali_nand |
| `0xFFB90000` | `0xFFB900FF` | 256 B | `hps_fpgamgr.axi_slave1` | altera_fpgamgr |
| `0xFFC00000` | `0xFFC00FFF` | 4 KB | `hps_dcan0.axi_slave0` | bosch_dcan |
| `0xFFC01000` | `0xFFC01FFF` | 4 KB | `hps_dcan1.axi_slave0` | bosch_dcan |
| `0xFFC02000` | `0xFFC020FF` | 256 B | `hps_uart0.axi_slave0` | snps_uart |
| `0xFFC03000` | `0xFFC030FF` | 256 B | `hps_uart1.axi_slave0` | snps_uart |
| `0xFFC04000` | `0xFFC040FF` | 256 B | `hps_i2c0.axi_slave0` | designware_i2c |
| `0xFFC05000` | `0xFFC050FF` | 256 B | `hps_i2c1.axi_slave0` | designware_i2c |
| `0xFFC06000` | `0xFFC060FF` | 256 B | `hps_i2c2.axi_slave0` | designware_i2c |
| `0xFFC07000` | `0xFFC070FF` | 256 B | `hps_i2c3.axi_slave0` | designware_i2c |
| `0xFFC08000` | `0xFFC080FF` | 256 B | `hps_timer0.axi_slave0` | dw_apb_timer_sp |
| `0xFFC09000` | `0xFFC090FF` | 256 B | `hps_timer1.axi_slave0` | dw_apb_timer_sp |
| `0xFFC25000` | `0xFFC25FFF` | 4 KB | `hps_sdrctl.axi_slave0` | altera_sdrctl |
| `0xFFD00000` | `0xFFD000FF` | 256 B | `hps_timer2.axi_slave0` | dw_apb_timer_osc |
| `0xFFD01000` | `0xFFD010FF` | 256 B | `hps_timer3.axi_slave0` | dw_apb_timer_osc |
| `0xFFD02000` | `0xFFD020FF` | 256 B | `hps_wd_timer0.axi_slave0` | dw_wd_timer |
| `0xFFD03000` | `0xFFD030FF` | 256 B | `hps_wd_timer1.axi_slave0` | dw_wd_timer |
| `0xFFD04000` | `0xFFD04FFF` | 4 KB | `hps_clkmgr.axi_slave0` | asimov_clkmgr |
| `0xFFD05000` | `0xFFD050FF` | 256 B | `hps_rstmgr.axi_slave0` | altera_rstmgr |
| `0xFFD08000` | `0xFFD083FF` | 1 KB | `hps_sysmgr.axi_slave0` | altera_sysmgr |
| `0xFFE01000` | `0xFFE01FFF` | 4 KB | `hps_dma.axi_slave0` | arm_pl330_dma |
| `0xFFF00000` | `0xFFF000FF` | 256 B | `hps_spim0.axi_slave0` | spi |
| `0xFFF01000` | `0xFFF010FF` | 256 B | `hps_spim1.axi_slave0` | spi |
| `0xFFFEC000` | `0xFFFEC0FF` | 256 B | `hps_scu.axi_slave0` | scu |
| `0xFFFEC100` | `0xFFFEC1FF` | 256 B | `hps_arm_gic_0.axi_slave1` | arm_gic |
| `0xFFFEC600` | `0xFFFEC6FF` | 256 B | `hps_timer.axi_slave0` | arm_internal_timer |
| `0xFFFED000` | `0xFFFEDFFF` | 4 KB | `hps_arm_gic_0.axi_slave0` | arm_gic |
| `0xFFFEF000` | `0xFFFEFFFF` | 4 KB | `hps_L2.axi_slave0` | arm_pl310_L2 |
| `0xFFFF0000` | `0xFFFFFFFF` | 64 KB | `hps_axi_ocram.axi_slave0` | axi_ocram |

## Visto desde `hps_bridges.h2f_lw`

| Base | Fin | Tamaño | Esclavo | Componente |
|---|---|---|---|---|
| `0x00000000` | `0x0000FFFF` | 64 KB | `shm.s2` | altera_avalon_onchip_memory2 |

## Interrupciones

| IRQ | Fuente | Receptor |
|---|---|---|
| 0 | `hps_dcan1.interrupt_sender1` | `hps_arm_gic_0.irq_rx_offset_136` |
| 0 | `hps_dma.interrupt_sender` | `hps_arm_gic_0.irq_rx_offset_104` |
| 0 | `hps_gpio2.interrupt_sender` | `hps_arm_gic_0.irq_rx_offset_166` |
| 1 | `hps_timer0.interrupt_sender` | `hps_arm_gic_0.irq_rx_offset_166` |
| 1 | `juart.irq` | `cpu.platform_irq_rx` |
| 2 | `hps_timer1.interrupt_sender` | `hps_arm_gic_0.irq_rx_offset_166` |
| 2 | `pio_key.irq` | `cpu.platform_irq_rx` |
| 3 | `hps_sdmmc.interrupt_sender` | `hps_arm_gic_0.irq_rx_offset_136` |
| 3 | `hps_timer2.interrupt_sender` | `hps_arm_gic_0.irq_rx_offset_166` |
| 3 | `pio_sw.irq` | `cpu.platform_irq_rx` |
| 4 | `audio.interrupt` | `cpu.platform_irq_rx` |
| 4 | `hps_timer3.interrupt_sender` | `hps_arm_gic_0.irq_rx_offset_166` |
| 5 | `hps_wd_timer0.interrupt_sender` | `hps_arm_gic_0.irq_rx_offset_166` |
| 6 | `hps_L2.interrupt_sender` | `hps_arm_gic_0.irq_rx_offset_32` |
| 6 | `hps_wd_timer1.interrupt_sender` | `hps_arm_gic_0.irq_rx_offset_166` |
| 8 | `hps_nand0.interrupt_sender` | `hps_arm_gic_0.irq_rx_offset_136` |
| 9 | `hps_fpgamgr.interrupt_sender` | `hps_arm_gic_0.irq_rx_offset_166` |
| 11 | `hps_gmac0.interrupt_sender` | `hps_arm_gic_0.irq_rx_offset_104` |
| 13 | `hps_timer.interrupt_sender` | `hps_arm_gic_0.arm_gic_ppi` |
| 15 | `hps_qspi.interrupt_sender` | `hps_arm_gic_0.irq_rx_offset_136` |
| 16 | `hps_gmac1.interrupt_sender` | `hps_arm_gic_0.irq_rx_offset_104` |
| 18 | `hps_spim0.interrupt_sender` | `hps_arm_gic_0.irq_rx_offset_136` |
| 19 | `hps_spim1.interrupt_sender` | `hps_arm_gic_0.irq_rx_offset_136` |
| 21 | `hps_usb0.interrupt_sender` | `hps_arm_gic_0.irq_rx_offset_104` |
| 22 | `hps_i2c0.interrupt_sender` | `hps_arm_gic_0.irq_rx_offset_136` |
| 23 | `hps_i2c1.interrupt_sender` | `hps_arm_gic_0.irq_rx_offset_136` |
| 24 | `hps_i2c2.interrupt_sender` | `hps_arm_gic_0.irq_rx_offset_136` |
| 24 | `hps_usb1.interrupt_sender` | `hps_arm_gic_0.irq_rx_offset_104` |
| 25 | `hps_i2c3.interrupt_sender` | `hps_arm_gic_0.irq_rx_offset_136` |
| 26 | `hps_uart0.interrupt_sender` | `hps_arm_gic_0.irq_rx_offset_136` |
| 27 | `hps_dcan0.interrupt_sender0` | `hps_arm_gic_0.irq_rx_offset_104` |
| 27 | `hps_uart1.interrupt_sender` | `hps_arm_gic_0.irq_rx_offset_136` |
| 28 | `hps_dcan0.interrupt_sender1` | `hps_arm_gic_0.irq_rx_offset_104` |
| 28 | `hps_gpio0.interrupt_sender` | `hps_arm_gic_0.irq_rx_offset_136` |
| 29 | `hps_gpio1.interrupt_sender` | `hps_arm_gic_0.irq_rx_offset_136` |
| 31 | `hps_dcan1.interrupt_sender0` | `hps_arm_gic_0.irq_rx_offset_104` |
