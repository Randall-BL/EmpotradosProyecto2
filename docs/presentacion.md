# Presentación funcional — guion

Guion de la demostración del reproductor en la DE1-SoC. Recorre el sistema en el orden en
que lo usaría una persona y, en cada paso, dice qué se muestra y qué se debe ver. Los
tiempos son aproximados (unos 20 minutos en total, más preguntas).

## Antes de empezar

| Qué | Detalle |
|---|---|
| Tarjeta | DE1-SoC con SW10 (MSEL) = `01010`, fuente, cable mini-USB "UART to USB" |
| microSD | Imagen de `scripts/make-sdimage.sh` con las 10 canciones (incluye WAV a 8, 16 y 44.1 kHz, mono y estéreo, ver `Musica/canciones.tsv`) |
| microSD de respaldo | Copia grabada de la misma imagen |
| Audio | Parlantes o audífonos en el line-out (verde) |
| Pantalla | Monitor VGA y cable |
| Volumen | Potenciómetro conectado al ADC (conector J15, CH0) |
| Laptop | Consola serial abierta (`sudo picocom -b 115200 /dev/ttyUSB0`), este repositorio, `docs/documento-diseno.pdf` y `docs/mapa-codigo.html` |
| Respaldo | Video de la demostración completa, por si falla el hardware |

Comprobar media hora antes: la tarjeta arranca sola, suena la primera canción y el
potenciómetro cambia el volumen. Dejar el volumen del parlante bajo antes de encender.

## 1. Arquitectura (3 min)

Con el diagrama del README o la figura 1 del documento de diseño:

- Partición: el **HPS** (Linux mínimo) lee la SD, extrae los metadatos y remuestrea a 48 kHz;
  el **Nios V** (bare metal, sin HAL) controla todo lo demás; el **hardware propio** hace el
  FIR y el volumen, y maneja los displays.
- Por qué así: el Nios V/m no tiene multiplicador. Un FIR de 127 taps en software costaría
  16 veces el procesador (medido con `tb_ciclos`); en hardware usa 4 DSP y 249 ALMs.
- El canal HPS ↔ Nios V es una RAM de doble puerto de 64 KB con un ring de 170 ms; el HPS
  lee por adelantado 4 MiB para que una lectura lenta de la SD no corte el audio.

## 2. Arranque (2 min)

| Acción | Se debe ver |
|---|---|
| Encender la tarjeta con la SD puesta, sin la laptop conectada al JTAG | En la consola: U-Boot, `init: SD montada`, `hps_player: ... 10 pistas, 0 descartadas` y `shm publicada (10 pistas)` |
| Cronometrar | Menos de 5 s hasta que la pantalla muestra la lista (requisito: < 10 s) |
| Mirar la VGA | Marco del reproductor con la canción 01 / 10, su título, artista, álbum y duración; estado DETENIDO |

## 3. Controles por interrupción (4 min)

| Acción | Se debe ver / oír |
|---|---|
| KEY0 | Empieza la canción; estado REPRODUCIENDO; LEDR7 encendido; HEX3..HEX0 cuenta MM:SS desde 00:00; HEX5..HEX4 = 01 |
| KEY0 | Pausa: el audio y el reloj se detienen; LEDR8 encendido |
| KEY0 | Reanuda en el mismo punto |
| KEY3 (siguiente) | Cambia en menos de 2 s; el reloj vuelve a 00:00; la VGA y HEX5..HEX4 muestran la nueva canción |
| KEY2 (anterior) en la canción 01 | Pasa a la 10 (lista circular) |
| KEY1 | Stop: silencio, reloj en 00:00, estado DETENIDO |
| Cualquier botón | LEDR6 cambia al instante: es la marca para medir la respuesta (< 200 ms; en simulación ≤ 2.2 ms) |

Explicar: la ISR solo lee y limpia `edgecapture` por puntero y encola el botón; el lazo
principal aplica la máquina de estados. Lo único del HAL es el registro de la interrupción
(`sw/niosv/drivers/irq.c`).

## 4. Metadatos y formatos (2 min)

- Pasar por varias canciones con KEY3 y señalar en la VGA título, artista, álbum,
  duración y "Canción n / 10".
- Mostrar *Jocelyn Flores* (8 kHz, mono) y *Querido Amigo* (16 kHz, mono) o *Loosing
  Interest* (16 kHz, estéreo): suenan a la velocidad correcta porque el HPS las convierte
  a 48 kHz. Las demás son de 44.1 kHz estéreo.

## 5. Filtros (4 min)

Con una canción que tenga graves y agudos:

| Switches | Se debe oír / ver |
|---|---|
| Todos abajo | Bypass: igual que el original; LEDR[2:0] apagados; "Filtro: BYPASS" en la VGA |
| SW0 | Pasa-bajos 1 kHz: solo graves, sonido apagado; LEDR0 |
| SW1 | Pasa-banda 1–4 kHz: sonido de teléfono o radio; LEDR1 |
| SW2 | Reverberación (eco de 50 ms) hecha en el Nios V; LEDR2 |
| Dos switches arriba | Combinación inválida: bypass, LEDR9 encendido y "SW INVALIDO" en la VGA |

El cambio se oye de inmediato (requisito < 100 ms; medido ≤ 41 ms). Si preguntan por la
evidencia: la respuesta en frecuencia medida en el RTL está en el README (figura) y la
verificación aislada en `docs/hw/verificacion-filtro.md` (1 253 comprobaciones bit a bit).

## 6. Volumen (1 min)

Girar el potenciómetro de un extremo al otro: el volumen sube en 32 escalones de 2 dB y la
barra de la VGA lo sigue. La ganancia la aplica el módulo propio (registro `GAIN`); en
software costaría el 52 % del Nios V.

## 7. Reproducción continua (1 min)

Adelantar con KEY3 hasta la canción 10, dejar que termine y mostrar que sigue sola con la
01. LEDR5 apagado al final: no hubo subdesbordes.

## 8. Hardware propio y verificación (3 min)

- `hw/rtl/filtro_audio.v`: mapa de registros (CTRL, STATUS, NTAPS, GAIN, COEF_*, DATA_IN,
  DATA_OUT) y `waitrequest`.
- `hw/rtl/fir_core.v`: dos MAC, acumulador de 40 bits, redondeo y saturación.
- Verificación aislada antes de integrar: Questa (`scripts/sim-rtl.sh filtro_audio`) y
  cocotb en la CI (`make docker-sim`, 16 pruebas). Si hay tiempo, correr una en vivo.
- Al arrancar, el Nios V autoprueba el módulo por sus registros (`filtro_selftest`).

## 9. Recursos, potencia y repositorio (1 min)

- 4 087 ALMs (13 %), 210 M10K (53 %), 4 DSP; 699.5 mW estimados; el filtro propio, 4.3 mW.
- Flujo Git: `main` protegida con check de CI obligatorio, `develop`, ramas por tarea,
  Conventional Commits, issues y tablero.

## Plan B

| Falla | Qué hacer |
|---|---|
| No arranca desde la SD | Probar la SD de respaldo. Si sigue, programar por JTAG (`scripts/program.sh`) y mostrar la consola |
| Sin audio | Revisar el cable en el line-out y el volumen del parlante; KEY1 y KEY0 de nuevo |
| Sin imagen VGA | Mostrar el estado por LEDs y displays; la pantalla en `docs/img/vga-simulada.png` |
| Falla el hardware | Pasar al video de respaldo y seguir con la explicación de la arquitectura y la verificación |

## Preguntas probables

| Pregunta | Respuesta corta |
|---|---|
| ¿Por qué el filtro en hardware y la reverberación en software? | El FIR multiplica 127 veces por muestra: en el Nios V/m (sin multiplicador) costaría 1 572 % del procesador. La reverberación solo suma y desplaza: 10 % |
| ¿Por qué se remuestrea en vez de cambiar la frecuencia del códec? | Con un solo MCLK el WM8731 no tiene 16 ni 44.1 kHz; a 48 kHz fijos hay un solo juego de coeficientes y el cambio de canción no toca el códec. El ARM tiene multiplicador y FPU (CD-07) |
| ¿Dónde se usa el HAL? | Solo para registrar y habilitar la interrupción de los botones (`irq.c`); `scripts/audit-hal.sh` lo comprueba en la CI |
| ¿Cómo se sincronizan el HPS y el Nios V? | Ring en la RAM de doble puerto; cada índice lo escribe un solo lado, así que no hace falta exclusión mutua. Los cambios de pista usan un número de secuencia y nunca se oyen restos de la canción anterior |
| ¿Cómo se verificó el filtro? | Aislado, solo por el bus, contra vectores dorados de un modelo bit a bit (0 LSB de diferencia), en Questa y en cocotb; además la respuesta en frecuencia medida en el RTL |
| ¿Por qué no hay cortes si la SD es lenta? | El kernel limitaba la espera de la SD a 100 ms y la reiniciaba; se parchó el driver y el HPS lee 4 MiB por adelantado en un hilo con prioridad de tiempo real (CD-02) |
| ¿Qué componente gasta más? | El HPS (131 mW) y la estática del dispositivo (422 mW); el hardware propio suma menos de 5 mW |
