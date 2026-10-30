# TODO — Proyecto II: SoC Reproductor de Audio (co-diseño HW/SW)

**Curso:** CE-1113 Sistemas Empotrados — TEC
**Plataforma:** Altera DE1-SoC · Platform Designer · Quartus Prime Lite 25.1std
**Fecha de entrega:** 2 de diciembre de 2026

> Leyenda: `[ ]` pendiente · `[~]` en progreso · `[x]` hecho
>
> **(OBL)** obligatorio · **(OPC)** opcional / crédito extra · **(EVID)** genera evidencia
> que debe subirse al repositorio. El número `#n` es el issue del board.
>
> ⚠️ **Restricción transversal:** el software **no puede usar el HAL** (`alt_*`,
> `IOWR`/`IORD`) — solo punteros directos a `system.h`. **Excepción única:** el registro
> y habilitación de interrupciones.

---

## 0. Arranque del proyecto

- [ ] Leer el enunciado completo y anotar dudas para el profesor
- [ ] Definir roles del grupo (HDL/DSP · SoC/Quartus · software bare metal · documentación)
- [ ] Revisar el Taller 9 (SoC con Nios V/m) como punto de partida
- [ ] Verificar instalación de Quartus Prime Lite 25.1std + Nios V/RiscFree + Questa
- [ ] Conseguir el material: DE1-SoC, tarjeta SD, potenciómetro, parlantes/audífonos, LCD/VGA

---

## 1. Repositorio y flujo de trabajo Git — 10% — #1, #2

- [x] Clonar el repositorio y crear ramas `main` / `develop` / `YitanDevelop`
- [x] Proteger `main` (merge solo vía Pull Request, sin force-push)
- [x] `CONTRIBUTING.md` con Conventional Commits, ramas y vinculación con issues
- [x] Etiquetas, hitos e issues de planeamiento en el board
- [x] `.gitignore` para Quartus, Platform Designer, Questa y BSP
- [ ] Estructura de directorios (`hw/`, `hw/tb/`, `soc/`, `software/`, `docs/`, `scripts/`) — #2
- [ ] Mantener el historial distribuido en el tiempo (no un único commit final)

---

## 2. Arquitectura y partición HW/SW — #3, #4

- [ ] Analizar el problema y los requerimientos (HW obligatorio / SW obligatorio / libres) — #3
- [ ] Elegir procesador: Nios V/m vs. HPS ARM Cortex-A9 (justificar) — #3
- [ ] Proponer al menos 2 alternativas de partición y compararlas (rendimiento, latencia,
      recursos, complejidad) — #3
- [ ] Decidir qué filtros van en HW y cuáles en SW — #3
- [ ] Decidir volumen (HW multiplicador vs. SW escalado), control temporal, visualización
      y máquina de reproducción (HW vs. SW) — #3
- [ ] Diagrama de arquitectura de hardware (bloques del SoC) — #4 (EVID)
- [ ] Diagrama de arquitectura de software (módulos, flujo, ISR) — #4 (EVID)
- [ ] Mapa de memoria preliminar (direcciones base de cada periférico) — #4

---

## 3. SoC base en Platform Designer — #5, #6, #7

- [ ] Sistema mínimo: procesador + memoria on-chip/SDRAM + JTAG UART + timer — #5 (OBL)
- [ ] PIO de botones KEY con interrupción por flanco — #5 (OBL)
- [ ] PIO de switches SW (selección de filtro) y LEDs (estado) — #5
- [ ] IP de Audio (WM8731) + Audio and Video Config — #6 (OBL)
- [ ] IP ADC Controller (AD7928) para el potenciómetro de volumen — #6 (OBL)
- [ ] Salida a displays 7 segmentos (PIO o controlador propio) — #7 (OBL)
- [ ] Interfaz de visualización (LCD / VGA / TFT) — #7 (OBL)
- [ ] Acceso a la tarjeta SD (SPI en FPGA o SD vía HPS, según la decisión) — #7 (OBL)
- [ ] Asignación de pines con el `.qsf` de la DE1-SoC y compilación limpia en Quartus — #5
- [ ] Justificar cada componente del SoC (no se permiten componentes sin función) — #5 (OBL)
- [ ] Prueba "hola mundo" bare metal: LEDs y 7 segmentos con punteros directos

---

## 4. Módulo de hardware personalizado y filtro DSP propio — #8, #9, #10

- [ ] Especificar el mapa de registros: control, estado, datos (entrada/salida de muestra,
      coeficientes, selección de filtro) — #8 (OBL)
- [ ] Interfaz Avalon-MM slave (mapeado a memoria) del módulo — #8 (OBL)
- [ ] Diseñar el filtro DSP propio en HDL (FIR pasa-bajos u otro): registros,
      multiplicadores/acumuladores, control de coeficientes — #9 (OBL)
- [ ] Calcular coeficientes (script Python/Octave) y cuantizarlos a punto fijo — #9
- [ ] Testbench en Questa con casos de prueba (impulso, escalón, senoidales) y
      comparación contra el modelo de referencia — #10 (OBL, EVID)
- [ ] Documentar casos de prueba y resultados antes de integrar — #10 (EVID)
- [ ] Empaquetar el módulo como componente de Platform Designer (`_hw.tcl`) — #8
- [ ] Diagramas de bloques y de tiempo del módulo — #8 (EVID)

---

## 5. Software bare metal del reproductor — #11 a #15

> Todo con punteros directos; HAL solo para registrar/habilitar interrupciones.

- [ ] Driver de la tarjeta SD (inicialización, lectura de bloques) — #11 (OBL)
- [ ] Lectura de FAT32: listar los ≥ 10 archivos `.wav` de la SD — #11 (OBL)
- [ ] Parser WAV: 16 bits, 8 kHz / 16 kHz / 44.1 kHz, canales, tamaño de datos — #12 (OBL)
- [ ] Extracción de metadatos (chunk `LIST/INFO`: título `INAM`, artista `IART`,
      álbum `IPRD`) y cálculo de la duración total — #12 (OBL)
- [ ] ISR de botones KEY: play/pausa, stop, anterior, siguiente — #13 (OBL)
- [ ] Antirrebote y respuesta < 200 ms — #13
- [ ] Máquina de estados de reproducción (reproduciendo / pausado / detenido) — #14 (OBL)
- [ ] Reproducción continua: pasar a la siguiente canción al terminar — #14 (OBL)
- [ ] Alimentación de la FIFO de la IP de audio sin cortes (buffer doble) — #14 (OBL)
- [ ] Interfaz de usuario: coordinación de todos los módulos en el lazo principal — #15 (OBL)
- [ ] Optimizar el mapa de memoria y el uso de buffers — #15 (OBL)
- [ ] Auditoría: `grep -rnE 'IOWR|IORD|alt_' software/` solo muestra interrupciones

---

## 6. Filtros, volumen y visualización — #16 a #19

- [ ] Filtro 1 (HW propio, del módulo personalizado) — #9 (OBL)
- [ ] Filtro 2 (pasa-banda / notch / pasa-altos IIR, SW o HW) — #16 (OBL)
- [ ] Filtro 3 (ecualizador de 3 bandas / reverberación, SW o HW) — #16 (OBL)
- [ ] Modo bypass sin distorsión audible — #16 (OBL)
- [ ] Selección de filtro con switches en tiempo real, latencia < 100 ms — #16 (OBL)
- [ ] Volumen por potenciómetro en un canal del ADC AD7928 — #17 (OBL)
- [ ] 7 segmentos con el tiempo transcurrido en MM:SS — #18 (OBL)
- [ ] LCD/VGA/TFT: título, artista, álbum, duración, número de canción (n/total),
      estado y filtro activo — #19 (OBL)
- [ ] Evidencias de funcionamiento de cada filtro (espectros/capturas/audio) — #16 (EVID)

---

## 7. Rendimiento, recursos y potencia — #20, #21

- [ ] Medir tiempo de arranque tras el reset (< 10 s) — #20 (OBL, EVID)
- [ ] Medir respuesta a botones (< 200 ms) y latencia de cambio de filtro (< 100 ms) — #20
- [ ] Medir transición entre canciones (< 2 s) — #20
- [ ] Prueba de estabilidad: reproducción continua de toda la lista sin reinicios — #20
- [ ] Reporte de utilización Quartus (ALMs, registros, memoria, DSP): sistema completo y
      módulo personalizado por separado — #21 (OBL, EVID)
- [ ] Estimación de potencia con PowerPlay Power Analyzer — #21 (OBL, EVID)
- [ ] Discusión del efecto de la partición HW/SW sobre recursos y potencia — #21 (OBL)

---

## 8. Documentación y entrega — 20% + 10% — #22 a #25

- [ ] Documento de diseño de software: métodos, bibliotecas, API, requisitos y lista de
      chequeo de cumplimiento (3%) — #22
- [ ] Metodología de diseño de sistema y justificación del co-diseño (7%) — #22
- [ ] Diseño del hardware personalizado: bloques, registros, mapa de memoria, diagramas
      de tiempo, justificación (4%) — #22
- [ ] Análisis de recursos y consumo (3%) — #22
- [ ] Herramientas de ingeniería: modelos, ecuaciones y scripts creados (3%) — #22
- [ ] README.md: instalación, compilación con Platform Designer, entorno, uso, diagramas,
      interfaz del módulo, mapas de memoria, evidencias de filtros, capturas y videos — #23 (EVID)
- [ ] Preparar 10+ canciones WAV de prueba con metadatos en la SD — #24
- [ ] Ensayo completo de la presentación funcional (60%) — #24
- [ ] PR final `develop` → `main` con `Closes #n` de todos los issues terminados — #25

---

## 9. Crédito extra (acordar con el profesor antes de empezar) — #26, #27

- [ ] Ecualizador de 5+ bandas en hardware DSP propio, ajustable en tiempo real — #26 (OPC)
- [ ] Transmisión/recepción de audio por UART o Bluetooth — #27 (OPC)
