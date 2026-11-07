# Pruebas en la tarjeta de la plataforma (IN-01..IN-04, PR-05)

Todo lo de esta página ya está verificado en simulación (tabla de
[README](../README.md#verificación-sin-tarjeta)); en la DE1-SoC se confirma lo que la
simulación no puede ver: pines, niveles analógicos, el códec real y el monitor.
Quien tenga la tarjeta corre cada prueba y anota el resultado y la evidencia (foto o
video) en la última columna.

## Preparación (una vez)

```bash
scripts/build-soc.sh && scripts/build-niosv.sh && scripts/build-quartus.sh
```

Para cada prueba: cargar el programa en el `.sof` y programar.

```bash
APP=<programa> scripts/update-sw.sh && scripts/program.sh
```

Conexiones: audífonos o parlantes en **LINE OUT** (verde), monitor en **VGA**,
potenciómetro de 10 kΩ entre 0 V y 5 V del conector del ADC con el cursor en **CH0**.

## Pruebas

| ID | Programa (`APP=`) | Pasos | Resultado esperado | Resultado |
|---|---|---|---|---|
| IN-01 / PR-05 | `test_pio` | Programar. Mover SW a 0x155 y volver a 0. Pulsar KEY0..KEY3, luego dos a la vez. Subir SW4 y SW3. | LEDR muestra 0x2AA al arrancar; luego 0x155 (eco de SW); 0x300 al quedar lista; cada KEY enciende LEDR8 + el LED de su número (0x101, 0x102, 0x104, 0x108); dos KEY juntos, ambos; SW4 → 0x210, SW3 → 0x218. Una pulsación con rebote se ve como una sola. | |
| PR-05 | `test_adc` | Girar el potenciómetro de un extremo al otro. | LEDR muestra el valor del ADC >> 2: todo apagado en un extremo, todo encendido en el otro, binario creciente en medio. Funciona igual en placas con AD7928 o con LTC2308. | |
| PR-05 | `test_vga` | Programar con el monitor conectado. | Pantalla del reproductor con "Cancion de prueba", "Artista de prueba", 03:45, canción 3 / 12, volumen 50 %. LEDR = 0x0B0. | |
| IN-02 | `test_tono` | Escuchar con SW0 = 0 y luego SW0 = 1. KEY0 silencia y vuelve a sonar. | Tono limpio de 440 Hz y de 1 kHz (sin chasquidos ni cortes). La pantalla muestra el tono y el estado. Si el códec no responde: mensaje de error y todos los LEDs encendidos. | |
| IN-03 | `test_tono` | Girar el potenciómetro. | El volumen cambia de forma pareja (escalones de 2 dB, silencio en un extremo); la barra de LEDs y el porcentaje en pantalla siguen al potenciómetro; no tiembla en reposo. | |
| IN-04 | `test_tono` | Mirar el monitor. | Marco, título de la aplicación, metadatos fijos, estado, filtro (tono) y barra de volumen como en el boceto de CD-09. | |
| PR-04 / NV-15 | `test_filtro` | Programar y mirar LEDR y los displays. | LEDR = 0x0D0 (autoprueba del módulo propio: 0 diferencias) y HEX5..HEX0 = `03 02:05`. Con 0x0DF falló la autoprueba. | |
| IN-08 | `test_filtros` | Escuchar con SW[1:0] = 00, 01, 10 y 11; cambiar los switches mientras suena. | 00: los tres tonos (300 Hz, 2.5 kHz, 8 kHz). 01: solo el grave. 10: solo el medio. 11: los tres con eco (con KEY1 se oyen pulsos y sus ecos cada 50 ms). El cambio es inmediato, sin chasquidos. HEX3..HEX0 cuenta MM:SS; la pantalla muestra el filtro; LEDR[1:0] = SW[1:0]; LEDR9 encendido si falló la autoprueba. | |
| PR-09 | `test_filtros` | SW = 00 y el potenciómetro al máximo; grabar el line-out en un PC. | Espectro con solo las tres líneas (sin armónicos de distorsión por encima de −60 dB respecto a las líneas). | |
| PR-10 | `test_filtros` | Repetir la grabación con SW = 01 y 10. | La línea de 8 kHz cae > 55 dB con el pasa-bajos; con el pasa-banda caen las de 300 Hz y 8 kHz. Comparar con `docs/img/filtro-respuesta-medida.png` (medida en simulación). | |

## Si algo falla

| Síntoma | Qué revisar |
|---|---|
| No programa | `quartus_pgm -l` debe listar el cable "DE-SoC"; probar `CABLE="..." scripts/program.sh` |
| LEDs bien pero sin audio | Line out (no mic); el error en pantalla indica I²C. Medir AUD_XCK (12.288 MHz) |
| Audio con ruido o mitad de volumen | Formato del códec: debe ser Left Justified 16 bits (R7 = 0x041) |
| Pantalla negra | Cable VGA; la sincronía es 640 × 480 a 60 Hz (verificada en `tb_vga`) |
| El volumen no cambia | Que el cursor esté en CH0 y la alimentación del potenciómetro entre 0 y 5 V |
