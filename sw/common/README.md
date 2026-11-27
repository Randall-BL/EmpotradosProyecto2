# `sw/common/` — lógica portable

Código C11 que no conoce el hardware: no incluye `system.h`, no toca registros, no usa
`malloc` y lee los datos little-endian byte a byte. Lo compilan el PC (pruebas en
`tests/`), el Nios V (`scripts/build-niosv.sh`) y el HPS (`make hps`).

```
sw/common/
├── include/   API pública (un header por módulo, con guarda COMMON_<MODULO>_H)
└── src/       implementación (funciones internas static)
```

## Prefijos de módulo

| Prefijo | Módulo | Tarea |
|---|---|---|
| `err_` / `ERR_` | Códigos de error comunes (`err.h`) | SC-01 |
| `le_` | Lectura little-endian con límites (`le.h`) | SC-02 |
| `wav_` | Parser RIFF/WAVE, metadatos y duración (`wav.h`) | SC-03..06 |
| `playlist_` | Lista de reproducción (`playlist.h`) | SC-07 |
| `fsm_` | Máquina de estados de reproducción (`fsm.h`) | SC-08 |
| `evq_` | Cola de eventos ISR → lazo principal (`evq.h`) | SC-09 |
| `port` (tipos `*_port_t`) | Puertos hacia el hardware (`ports.h`) | SC-14 |
| `shm_` | Protocolo y ring buffer HPS ↔ Nios V (`shm_proto.h`) | SC-13 (borrador) |
| `shm_source_` | Lado Nios V del canal como puerto de fuente (`shm_source.h`) | NV-13 |
| `hps_model_` | Modelo del productor del HPS para probar sin tarjeta (`hps_model.h`) | — |
| `sw_` | Switches → filtro (`switches.h`) | CD-08, NV-05 |
| `deb_` | Antirrebote por ventana de tiempo (`debounce.h`) | NV-04 |
| `clk_` | Acumulador de milisegundos (`clock.h`) | NV-12 |
| `app_` | Lazo principal del reproductor (`app.h`) | NV-14 |
| `dsp_`, `volume_`, `screen_`, `tone_` | Filtros, volumen, pantalla y tono | SC-10, SC-11, NV-11 |

## Códigos de error

Las funciones que pueden fallar devuelven `err_t` (`ERR_OK` = 0, los errores son
negativos) y entregan el resultado por puntero. `err_str()` da un texto corto para la
pantalla o la UART.
