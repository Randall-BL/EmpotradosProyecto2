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

## Interrupciones

| IRQ | Fuente | Receptor |
|---|---|---|
| 1 | `juart.irq` | `cpu.platform_irq_rx` |
| 2 | `pio_key.irq` | `cpu.platform_irq_rx` |
| 3 | `pio_sw.irq` | `cpu.platform_irq_rx` |
| 4 | `audio.interrupt` | `cpu.platform_irq_rx` |
