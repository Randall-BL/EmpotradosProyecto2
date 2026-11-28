#!/usr/bin/env python3
"""Genera docs/mapa-codigo.html: navegador del codigo organizado por los requisitos
del enunciado (proyecto-2-reproductor-audio-2026ii.pdf).

Las referencias se resuelven por patron (regex) al generar, asi que los numeros de
linea siguen siendo correctos aunque el codigo cambie. Uso:

    python3 scripts/gen-mapa-codigo.py      # luego abrir docs/mapa-codigo.html
"""
import json
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "docs" / "mapa-codigo.html"
SKIP_EXT = {".png", ".pdf", ".mp3", ".jpg", ".zip"}
SKIP_PREFIX = ("Musica/", "sim/vectors/fir_")
MAX_BYTES = 120_000

# --------------------------------------------------------------------------- archivos
files = {}
for p in subprocess.check_output(["git", "ls-files"], cwd=ROOT, text=True).split("\n"):
    if not p or Path(p).suffix.lower() in SKIP_EXT or p.startswith(SKIP_PREFIX):
        continue
    if p.endswith("mapa-codigo.html"):
        continue
    f = ROOT / p
    try:
        data = f.read_bytes()
    except OSError:
        continue
    if b"\0" in data[:4096] or len(data) > MAX_BYTES:
        continue
    files[p] = data.decode("utf-8", "replace")

missing = []


def span_end(lines, i, path, n):
    """Ultima linea (0-based) del bloque que empieza en i."""
    if n:
        return min(i + n - 1, len(lines) - 1)
    ext = Path(path).suffix
    if ext in (".c", ".h"):
        depth, seen = 0, False
        for j in range(i, min(i + 400, len(lines))):
            depth += lines[j].count("{") - lines[j].count("}")
            if "{" in lines[j]:
                seen = True
            if seen and depth <= 0:
                return j
            if not seen and lines[j].rstrip().endswith(";"):
                return j
        return i
    if ext in (".v", ".sv"):
        if re.match(r"\s*module\b", lines[i]):
            for j in range(i, len(lines)):
                if re.match(r"\s*endmodule\b", lines[j]):
                    return j
        depth = 0
        for j in range(i, min(i + 200, len(lines))):
            depth += len(re.findall(r"\bbegin\b", lines[j])) - len(re.findall(r"\bend\b", lines[j]))
            if j > i and depth <= 0:
                return j
        return i
    return i


def L(path, pattern=None, label=None, n=0):
    """Referencia a un archivo; con pattern, al bloque que empieza en esa linea."""
    if path not in files:
        missing.append(path)
        return None
    ref = {"f": path, "t": label or path.split("/")[-1]}
    if pattern:
        lines = files[path].split("\n")
        rx = re.compile(pattern)
        for i, line in enumerate(lines):
            if rx.search(line):
                ref["a"] = i + 1
                ref["b"] = span_end(lines, i, path, n) + 1
                break
        else:
            missing.append(f"{path} :: {pattern}")
    return ref


def R(req, cite, status, how, code, tests=(), docs=()):
    return {
        "req": req, "cite": cite, "st": status, "how": how,
        "code": [x for x in code if x], "tests": [x for x in tests if x],
        "docs": [x for x in docs if x],
    }


APP = "sw/common/src/app.c"
TCL = "hw/soc/build_system.tcl"
FIR = "hw/rtl/fir_core.v"
FA = "hw/rtl/filtro_audio.v"
S7 = "hw/rtl/seg7_time.v"

SECTIONS = [
    {"id": "contenido", "title": "Gestión de contenido musical", "cite": "§3 p.3", "items": [
        R("Al menos 10 canciones en la SD", "§3 · Gestión de contenido", "ok",
          "El HPS recorre el directorio de la SD, valida cada WAV y arma la tabla de pistas "
          "que publica en la memoria compartida. La playlist del Nios V comprueba el mínimo.",
          [L("sw/hps/src/producer.c", r"^uint32_t producer_scan", "producer_scan(): recorre la SD"),
           L("sw/hps/src/producer.c", r"^void producer_publish", "producer_publish(): tabla en shm"),
           L("sw/common/src/playlist.c", r"^int playlist_meets_minimum", "playlist_meets_minimum()"),
           L("scripts/convert-music.sh", None, "convert-music.sh (prepara los WAV)"),
           L("scripts/make-sdimage.sh", None, "make-sdimage.sh (imagen de la SD)")],
          [L("sim/questa/tb_player.sv", None, "tb_player (10 pistas)"),
           L("tests/test_app.c", None, "test_app (dos vueltas a 10 pistas)")],
          [L("docs/pruebas-tarjeta.md", r"HP-05, HP-06", "Prueba en tarjeta: 10 pistas")]),
        R("WAV 16 bits a 8 / 16 / 44.1 kHz", "§3 · Formatos de audio", "ok",
          "wav.c analiza la cabecera RIFF (fmt, data, chunks impares, EXTENSIBLE). El códec va "
          "fijo a 48 kHz (CD-07) y el HPS remuestrea por interpolación lineal antes de "
          "escribir en el ring.",
          [L("sw/common/src/wav.c", r"^err_t wav_parse", "wav_parse(): recorre los chunks RIFF"),
           L("sw/common/src/wav.c", r"^static err_t parse_fmt", "parse_fmt(): chunk fmt"),
           L("sw/common/src/wav.c", r"^static err_t check_fmt", "check_fmt(): formatos aceptados"),
           L("sw/common/src/resample.c", r"^uint32_t resample_run", "resample_run(): a 48 kHz"),
           L("sw/hps/src/producer.c", r"uint32_t made = resample_run", "producer.c: uso del remuestreo", 3)],
          [L("tests/test_wav.c", None, "test_wav (26 WAV generados)"),
           L("tests/test_resample.c", None, "test_resample"),
           L("tests/data/gen_wavs.py", None, "gen_wavs.py (genera los casos)")],
          [L("docs/decisiones/CD-07.md", None, "CD-07 Frecuencias")]),
        R("Metadatos: título, artista, álbum y duración", "§3 · Metadatos (y §3-SW Parseo)", "ok",
          "Se leen de LIST/INFO y de ID3 dentro del WAV; si faltan se usa el nombre del archivo. "
          "La duración sale de data_size / byte_rate.",
          [L("sw/common/src/wav.c", r"^static void parse_info", "parse_info(): LIST/INFO"),
           L("sw/common/src/wav.c", r"^static void parse_id3", "parse_id3(): etiquetas ID3"),
           L("sw/common/src/wav.c", r"^void wav_meta_fallback", "wav_meta_fallback()"),
           L("sw/common/src/wav.c", r"^uint32_t wav_duration_s", "wav_duration_s()"),
           L("sw/hps/src/producer.c", r"^static int load_track", "load_track(): fila de la tabla"),
           L("sw/common/include/shm_proto.h", r"typedef struct", "shm_track_t (formato en shm)", 14)],
          [L("tests/test_wav.c", None, "test_wav")]),
        R("Lista de reproducción: anterior / siguiente, continua", "§3 · Lista de reproducción", "ok",
          "playlist.c lleva el índice con vuelta al inicio/final; la FSM decide cuándo navegar.",
          [L("sw/common/src/playlist.c", r"^uint32_t playlist_next", "playlist_next()"),
           L("sw/common/src/playlist.c", r"^uint32_t playlist_prev", "playlist_prev()"),
           L("sw/common/src/playlist.c", r"^int playlist_auto_advance", "playlist_auto_advance()")],
          [L("tests/test_fsm.c", None, "test_fsm")]),
    ]},
    {"id": "control", "title": "Control de reproducción", "cite": "§3 p.3", "items": [
        R("Play, pausa, stop, anterior, siguiente", "§3 · Controles básicos", "ok",
          "Máquina de estados por tabla (DETENIDO / REPRODUCIENDO / PAUSADO × 6 eventos). "
          "KEY0..KEY3 se traducen a eventos con KEY_EVENT[] y apply() ejecuta las acciones.",
          [L("sw/common/src/fsm.c", r"^static const transition_t table", "Tabla de transiciones"),
           L("sw/common/src/fsm.c", r"^uint32_t fsm_handle", "fsm_handle()"),
           L(APP, r"^static const fsm_event_t KEY_EVENT", "KEY_EVENT[]: KEY → evento", 6),
           L(APP, r"^static void apply", "apply(): ejecuta las acciones de la FSM")],
          [L("tests/test_fsm.c", None, "test_fsm (10 000 eventos aleatorios)"),
           L("sim/questa/tb_player.sv", None, "tb_player")],
          [L("docs/decisiones/CD-11.md", None, "CD-11 Máquina de reproducción")]),
        R("Botones con respuesta por interrupciones", "§3 · Interfaz física (y §3-SW Interrupción)", "ok",
          "El PIO de KEY dispara la IRQ 2 del Nios V. La ISR solo lee/limpia edgecapture por "
          "puntero directo y encola; el lazo principal vacía la cola con antirrebote. El "
          "registro de la ISR es la única excepción permitida al HAL (irq.c).",
          [L("sw/niosv/drivers/keys.c", r"^static void keys_isr", "keys_isr(): la ISR"),
           L("sw/niosv/drivers/keys.c", r"^int keys_init", "keys_init(): registra y habilita"),
           L("sw/niosv/drivers/irq.c", r"^int irq_attach", "irq.c: alt_ic_isr_register (excepción)"),
           L("sw/niosv/drivers/pio.h", r"pio_edges_take", "pio_edges_take(): edgecapture", 6),
           L("sw/common/src/evq.c", r"^err_t evq_push", "evq_push(): cola ISR → lazo"),
           L(TCL, r"^irq pio_key.irq", "build_system.tcl: IRQ del PIO KEY", 1),
           L(APP, r"^static void handle_events", "handle_events(): consume la cola")],
          [L("sim/questa/tb_pio.sv", None, "tb_pio (IRQ por KEY, 17 µs)"),
           L("tests/test_input.c", None, "test_input")]),
        R("Indicación del estado (reproduciendo / pausado / detenido)", "§3 · Estado de reproducción", "ok",
          "Estado en la VGA (fila de estado) y en LEDR7 (reproduciendo) / LEDR8 (pausado).",
          [L(APP, r"^static uint32_t leds_value", "leds_value(): LEDs de estado"),
           L("sw/common/src/screen.c", r"^void screen_draw_status", "screen_draw_status()"),
           L("sw/common/src/fsm.c", r"^const char \*fsm_state_name", "fsm_state_name()")]),
        R("Al terminar una canción, sigue la siguiente", "§3 · Reproducción continua", "ok",
          "Cuando la fuente reporta SRC_ENDED, pump_audio() envía FSM_EV_TRACK_END y la tabla "
          "hace NAV_AUTO + LOAD + START.",
          [L(APP, r"SRC_ENDED\)", "pump_audio(): detecta el fin de pista", 4),
           L("sw/common/src/fsm.c", r"\[FSM_EV_TRACK_END\]  = \{ P", "Transición TRACK_END en REPRODUCIENDO", 1)],
          [L("sim/questa/tb_player.sv", None, "tb_player (avance automático, dos vueltas)")]),
    ]},
    {"id": "visual", "title": "Visualización de información", "cite": "§3 p.3–4", "items": [
        R("Tiempo transcurrido MM:SS en 7 segmentos", "§3 · Tiempo de reproducción (y §3-HW 7 seg.)", "ok",
          "Hardware propio seg7_time: el software escribe los segundos y el módulo convierte a "
          "MM:SS por restas y maneja HEX3..HEX0 directamente. El tiempo cuenta cuadros enviados "
          "al códec (CD-10), así se detiene solo en pausa.",
          [L(S7, r"^module seg7_time", "seg7_time.v (módulo completo)"),
           L(S7, r"Convertidor secuencial", "Conversión a MM:SS por restas", 35),
           L(S7, r"case \(avs_address\)", "Registros Avalon (escritura)", 6),
           L("sw/niosv/drivers/seg7.c", r"^void seg7_set_time", "seg7_set_time()"),
           L(APP, r"^static void count_frame", "count_frame(): segundos = cuadros / 48 000"),
           L("hw/quartus/DE1_SoC_top.v", r"\.hex_hex0", "Top-level: HEX0..HEX5", 6)],
          [L("sim/questa/rtl/tb_seg7_time.sv", None, "tb_seg7_time (aislado)")],
          [L("docs/hw/seg7-time.md", None, "docs/hw/seg7-time.md"),
           L("docs/decisiones/CD-10.md", None, "CD-10 Tiempo")]),
        R("Metadatos en interfaz externa (VGA)", "§3 · Información de canción", "ok",
          "Character Buffer 80×60 de Platform Designer + controlador VGA. vga_text.c escribe "
          "las celdas por puntero; screen.c arma la pantalla; app.c decide qué redibujar.",
          [L("sw/niosv/drivers/vga_text.c", r"^void vga_text_print", "vga_text_print()"),
           L("sw/common/src/screen.c", r"^void screen_draw_track", "screen_draw_track(): título, artista, álbum, duración"),
           L(APP, r"^static void draw_track", "draw_track()"),
           L(APP, r"^static void ui_refresh", "ui_refresh(): redibujo por partes"),
           L(TCL, r"^add_instance char_buf", "SoC: char_buf / video_fifo / vga", 12)],
          [L("sim/questa/tb_vga.sv", None, "tb_vga (640×480 y texto)"),
           L("tests/test_screen.c", None, "test_screen")],
          [L("docs/decisiones/CD-09.md", None, "CD-09 Visualización")]),
        R("Número de canción actual y total", "§3 · Indicador de canción", "ok",
          "«Canción NN / NN» en la VGA y el número en HEX5..HEX4 (seg7_time, registro TRACK).",
          [L("sw/common/src/screen.c", r"Cancion  NN / NN", "screen.c: «Canción n / total»", 6),
           L("sw/niosv/drivers/seg7.c", r"^void seg7_set_track", "seg7_set_track(): HEX5..HEX4")]),
        R("Indicación del filtro activo", "§3 · Estado de filtros", "ok",
          "Nombre del filtro en la fila de estado de la VGA y LEDR[2:0]; LEDR9 si la combinación "
          "de switches es inválida.",
          [L("sw/common/src/switches.c", r"^const char \*sw_filter_name", "sw_filter_name()"),
           L(APP, r"^static uint32_t leds_value", "leds_value(): LEDR[2:0] y LEDR9"),
           L("sw/common/src/screen.c", r"^void screen_draw_status", "screen_draw_status()")]),
    ]},
    {"id": "audio", "title": "Procesamiento de audio", "cite": "§3 p.4 y §3-SIS p.5", "items": [
        R("3 filtros seleccionables (al menos 1 en HDL propio)", "§3 · Filtros / §3-SIS", "ok",
          "Pasa-bajos FIR 1 kHz y pasa-banda FIR 1–4 kHz en el módulo propio (fir_core: 127 taps, "
          "2 MAC en paralelo, acumulador de 40 bits, 2 bancos de coeficientes); reverberación en "
          "el Nios V (sumas y desplazamientos). Coeficientes diseñados con design_fir.py.",
          [L(FIR, r"^module fir_core", "fir_core.v: el FIR en HDL"),
           L(FIR, r"localparam S_CLEAR", "FSM del FIR (CLEAR→LOAD→MAC→DRAIN→ROUND→GAIN)", 2),
           L(FIR, r"Pipeline: ciclo 1", "MAC: multiplicadores y acumuladores", 15),
           L(FIR, r"function signed \[15:0\] sat16", "Redondeo y saturación", 8),
           L("hw/rtl/fir_coefs.hex", None, "fir_coefs.hex (coeficientes)"),
           L("scripts/design_fir.py", None, "design_fir.py (diseño de los FIR)"),
           L("sw/common/src/dsp.c", r"^int16_t dsp_reverb_step", "dsp_reverb_step(): reverberación SW"),
           L(APP, r"int reverb = ", "pump_audio(): aplica la reverberación", 14),
           L("sw/niosv/drivers/ports_niosv.c", r"^static void audio_write_port", "audio_write_port(): muestra → filtro HW → códec")],
          [L("sim/questa/rtl/tb_filtro_audio.sv", None, "tb_filtro_audio (8 casos dorados)"),
           L("sim/questa/rtl/tb_filtro_respuesta.sv", None, "tb_filtro_respuesta (respuesta en frecuencia)"),
           L("tests/test_dsp.c", None, "test_dsp"),
           L("sim/questa/tb_ciclos.sv", None, "tb_ciclos (costo en el Nios V)")],
          [L("docs/decisiones/CD-12.md", None, "CD-12 Filtros"),
           L("docs/decisiones/CD-14.md", None, "CD-14 Punto fijo")]),
        R("Selección por switches en tiempo real", "§3 · Selección de filtros", "ok",
          "Se encuestan los switches cada 20 ms con confirmación; un switch por filtro, dos o más "
          "= inválido (bypass). Al cambiar se reprograma el módulo (bypass/banco).",
          [L("sw/common/src/switches.c", r"^filter_id_t sw_decode", "sw_decode(): SW → filtro"),
           L("sw/common/src/switches.c", r"^int sw_update", "sw_update(): confirmación"),
           L(APP, r"^static void poll_inputs", "poll_inputs(): cada SW_POLL_MS"),
           L("sw/niosv/drivers/ports_niosv.c", r"^static void fx_filter_port", "fx_filter_port(): filtro → bypass + banco"),
           L("sw/niosv/drivers/filtro.c", r"^void filtro_config", "filtro_config()")],
          [L("tests/test_input.c", None, "test_input")],
          [L("docs/decisiones/CD-08.md", None, "CD-08 Switches")]),
        R("Bypass sin distorsión", "§3 · Calidad de audio", "ok",
          "En bypass el FIR no hace MAC (y = x) y solo se aplica la ganancia con redondeo y "
          "saturación.",
          [L(FIR, r"if \(bypass_lat\) begin", "S_LOAD: en bypass salta el MAC (y = x)", 6),
           L(FIR, r"S_GAIN: begin", "S_GAIN: solo ganancia con saturación", 6),
           L("sw/niosv/drivers/filtro.c", r"^static uint32_t test_bypass_gain", "Autoprueba: bypass + ganancia")],
          [L("sim/questa/tb_audio.sv", None, "tb_audio (sin pérdidas)"),
           L("tests/test_tone.c", None, "test_tone (SNR 48.9 dB)")]),
        R("Cambio de filtro < 100 ms", "§3 · Latencia", "ok",
          "Encuesta de 20 ms + confirmación; medido ≤ 41 ms en test_app.",
          [L("sw/common/include/switches.h", r"#define SW_POLL_MS", "SW_POLL_MS = 20 ms", 1)],
          [L("tests/test_app.c", r"^static void test_filters_and_volume", "test_filters_and_volume(): < 100 ms")]),
        R("Volumen con potenciómetro por el ADC", "§3-SIS · Control de volumen", "ok",
          "adc.c lee CH0; volume.c convierte a 32 escalones de 2 dB con histéresis; la ganancia "
          "Q1.15 se escribe en el registro GAIN del módulo propio (multiplicador en HW, CD-05).",
          [L("sw/niosv/drivers/adc.c", r"^uint32_t adc_read", "adc_read()"),
           L("sw/common/src/volume.c", r"^int volume_update", "volume_update(): escalón con histéresis"),
           L("sw/niosv/drivers/filtro.c", r"^void filtro_set_gain", "filtro_set_gain()"),
           L(FIR, r"S_GAIN: begin", "fir_core: etapa S_GAIN (y·g/32768)", 6),
           L(TCL, r"^add_instance adc", "SoC: ADC Controller", 8)],
          [L("sim/questa/tb_adc.sv", None, "tb_adc (AD7928 y LTC2308)"),
           L("tests/test_volume.c", None, "test_volume")],
          [L("docs/decisiones/CD-05.md", None, "CD-05 Volumen")]),
    ]},
    {"id": "rend", "title": "Rendimiento del sistema", "cite": "§3 p.4", "items": [
        R("Operativo en < 10 s tras el reset", "§3 · Tiempo de inicio", "ok",
          "Nios V: main() inicializa todo y entra al lazo. HPS: U-Boot → Linux mínimo → init "
          "monta la SD y lanza hps_player. Medido en la tarjeta: menos de 5 s.",
          [L("sw/niosv/src/main.c", r"^int main", "Nios V: main()"),
           L("sw/hps/linux/init", None, "HPS: script init"),
           L("sw/hps/linux/boot.cmd", None, "boot.cmd (U-Boot)")],
          docs=[L("docs/pruebas-tarjeta.md", r"PR-07 arranque", "Medición en la tarjeta (PR-07)")]),
        R("Respuesta a botones < 200 ms", "§3 · Respuesta a botones", "ok",
          "ISR + cola + lazo no bloqueante: 17 µs en tb_pio, ≤ 2.2 ms en tb_player.",
          [L(APP, r"^void app_step", "app_step(): lazo principal no bloqueante")],
          [L("sim/questa/tb_pio.sv", None, "tb_pio"),
           L("sim/questa/tb_player.sv", None, "tb_player")]),
        R("Reproducción continua sin interrupciones", "§3 · Estabilidad", "ok",
          "El HPS lee la SD por adelantado en un hilo (prefetch) y llena el ring de shm; el "
          "Nios V detecta y reporta subdesbordes (LEDR5).",
          [L("sw/hps/src/prefetch.c", r"^static void \*reader", "prefetch.c: hilo lector"),
           L("sw/hps/src/producer.c", r"^static int stream", "stream(): llena el ring"),
           L("sw/hps/src/producer.c", r"^static void check_starving", "check_starving()"),
           L(APP, r"^static void pump_audio", "pump_audio(): FIFO del códec + subdesbordes")],
          [L("tests/test_producer.c", None, "test_producer"),
           L("tests/test_app.c", None, "test_app (dos vueltas sin subdesbordes)")]),
        R("Cambio de canción < 2 s", "§3 · Cambio de canción", "ok",
          "PLAY por shm → el HPS abre la pista y entrega el primer bloque; medido < 2 s en test_app.",
          [L("sw/hps/src/producer.c", r"^static uint32_t start_track", "start_track()"),
           L("sw/common/src/shm_source.c", r"^static void src_play", "src_play(): comando PLAY")],
          [L("tests/test_app.c", None, "test_app")]),
    ]},
    {"id": "hw", "title": "Requerimientos de hardware (solo HW)", "cite": "§3-HW p.4–5", "items": [
        R("Módulo propio con registros de control, estado y datos mapeados a memoria", "§3-HW", "ok",
          "filtro_audio: esclavo Avalon-MM con CTRL, STATUS, NTAPS, GAIN, COEF_ADDR, COEF_DATA, "
          "DATA_IN, DATA_OUT y waitrequest. Base 0x0004_0100. Empaquetado con _hw.tcl.",
          [L(FA, r"^// Modulo de hardware personalizado", "Mapa de registros (cabecera)", 23),
           L(FA, r"localparam A_CTRL", "Direcciones de registro", 2),
           L(FA, r"assign avs_waitrequest", "waitrequest", 2),
           L(FA, r"A_CTRL:   begin bypass", "Escritura de registros", 8),
           L(FA, r"A_CTRL:   avs_readdata", "Lectura de registros", 8),
           L("hw/rtl/filtro_audio_hw.tcl", None, "filtro_audio_hw.tcl (componente)"),
           L(TCL, r"^map cpu.data_manager        filtro.s1", "Dirección base en el SoC", 1),
           L("sw/niosv/drivers/filtro.h", None, "Driver: filtro.h")],
          docs=[L("docs/hw/modulo-filtro.md", None, "docs/hw/modulo-filtro.md (bloques, registros, tiempos)")]),
        R("Verificación aislada del módulo antes de integrarlo", "§3-HW · Verificación independiente", "ok",
          "Testbench Questa solo por el bus Avalon contra vectores dorados del modelo bit a bit "
          "(1 253 comprobaciones), respuesta en frecuencia, y además una rutina en SW "
          "(filtro_selftest) que corre al arrancar.",
          [L("sim/questa/rtl/tb_filtro_audio.sv", None, "tb_filtro_audio.sv"),
           L("sim/questa/rtl/tb_filtro_respuesta.sv", None, "tb_filtro_respuesta.sv"),
           L("sim/questa/rtl/tb_seg7_time.sv", None, "tb_seg7_time.sv"),
           L("scripts/sim-rtl.sh", None, "sim-rtl.sh"),
           L("scripts/plot_vectors.py", None, "plot_vectors.py"),
           L("sw/niosv/drivers/filtro.c", r"^uint32_t filtro_selftest", "filtro_selftest(): rutina SW"),
           L("sim/cocotb/filtro_audio/test_registros.py", None, "cocotb: test_registros.py (CI)"),
           L("sim/cocotb/filtro_audio/test_filtro.py", None, "cocotb: test_filtro.py (CI)"),
           L("sim/cocotb/seg7_time/test_seg7_time.py", None, "cocotb: test_seg7_time.py (CI)")],
          [L("docs/verificacion/resultados/rtl_filtro_audio.txt", None, "Resultado rtl_filtro_audio"),
           L("docs/verificacion/resultados/rtl_filtro_respuesta.txt", None, "Resultado rtl_filtro_respuesta"),
           L("docs/verificacion/resultados/rtl_seg7_time.txt", None, "Resultado rtl_seg7_time")],
          [L("docs/hw/verificacion-filtro.md", None, "docs/hw/verificacion-filtro.md (casos y resultados)"),
           L("sim/vectors/README.md", None, "sim/vectors/README.md")]),
        R("Audio manejado por la IP de Audio en la FPGA", "§3-HW · Interfaz de audio", "ok",
          "audio_pll + audio + av_config en Platform Designer; el HPS no toca el códec.",
          [L(TCL, r"^add_instance audio_pll", "SoC: audio_pll, audio, av_config", 14),
           L("sw/niosv/drivers/audio.c", r"^void audio_write_frame", "audio_write_frame()"),
           L("sw/niosv/drivers/audio.c", r"^uint32_t audio_space", "audio_space()"),
           L("sw/niosv/drivers/codec.c", r"^int codec_init", "codec_init(): I²C del WM8731")],
          [L("sim/questa/tb_audio.sv", None, "tb_audio"),
           L("sim/questa/tb_codec.sv", None, "tb_codec"),
           L("sim/questa/wm8731_model.sv", None, "Modelo del WM8731")]),
        R("Displays de 7 segmentos manejados directamente", "§3-HW · 7 segmentos", "ok",
          "seg7_time decodifica y saca los segmentos a HEX5..HEX0 desde la FPGA.",
          [L(S7, r"case \(d\)", "Decodificador BCD → segmentos", 6),
           L(TCL, r"^add_instance seg7", "SoC: seg7", 1)]),
        R("Cada componente con función justificada", "§3-HW", "ok",
          "Tabla instancia → función → requisito → costo.",
          [L(TCL, r"^add_instance clk", "build_system.tcl: todas las instancias", 160)],
          docs=[L("docs/hw/componentes-soc.md", None, "docs/hw/componentes-soc.md")]),
    ]},
    {"id": "sw", "title": "Requerimientos de software (solo SW)", "cite": "§3-SW p.5", "items": [
        R("SD manejada por el HPS", "§3-SW · Control de almacenamiento", "ok",
          "Linux mínimo en el ARM monta la SD (FAT, solo lectura) y hps_player lee los WAV; "
          "accede a la shm por /dev/mem a través del puente liviano H2F.",
          [L("sw/hps/src/main.c", r"^int main", "hps_player: main()"),
           L("sw/hps/src/producer.c", None, "producer.c"),
           L("sw/hps/src/devmem.c", r"^int devmem_open", "devmem_open(): mapea el puente"),
           L(TCL, r"^add_instance hps", "SoC: HPS", 6)]),
        R("Interrupciones para los botones", "§3-SW · Sistema de interrupción", "ok",
          "Ver «Botones con respuesta por interrupciones».",
          [L("sw/niosv/drivers/keys.c", r"^static void keys_isr", "keys_isr()"),
           L("sw/niosv/drivers/irq.c", None, "irq.c")]),
        R("Parseo de metadatos", "§3-SW", "ok", "Ver «Metadatos».",
          [L("sw/common/src/wav.c", r"^err_t wav_parse", "wav_parse()")]),
        R("Interfaz de usuario: coordina todo", "§3-SW", "ok",
          "app_step(): eventos de botones → entradas (SW/ADC) → fuente/audio → pantalla.",
          [L(APP, r"^void app_step", "app_step()"),
           L(APP, r"^void app_init", "app_init()"),
           L("sw/common/include/ports.h", None, "ports.h: puertos (permite probar en el PC)")]),
        R("Gestión / optimización del mapa de memoria", "§3-SW", "ok",
          "Todo on-chip: 128 KB de RAM del Nios V y 64 KB de doble puerto compartidos con el HPS; "
          "protocolo de ring en shm.",
          [L(TCL, r"^map cpu.data_manager        ram.s1", "Mapa de direcciones", 18),
           L("sw/common/include/shm_proto.h", None, "shm_proto.h: estructura de la shm"),
           L("scripts/report-memory.sh", None, "report-memory.sh")],
          [L("sim/questa/tb_shm.sv", None, "tb_shm"), L("tests/test_shm.c", None, "test_shm")],
          [L("docs/hw/mapa-memoria.md", None, "docs/hw/mapa-memoria.md"),
           L("docs/decisiones/CD-06.md", None, "CD-06 Memoria")]),
        R("Nios V con punteros directos, sin HAL ni IOWR/IORD", "§3-SW", "ok",
          "HW_REG() es un puntero volatile a base + offset de system.h. La auditoría falla si "
          "aparece alt_*/IOWR/IORD fuera de irq.c.",
          [L("sw/niosv/drivers/hw.h", r"#define HW_REG\(", "HW_REG(): puntero volatile", 1),
           L("sw/niosv/drivers/pio.h", r"PIO_REG_DATA", "pio.h: offsets del PIO", 30),
           L("sw/niosv/drivers/irq.c", None, "irq.c: única excepción (interrupciones)"),
           L("scripts/audit-hal.sh", None, "audit-hal.sh")]),
        R("HPS con SO mínimo, sin interacción", "§3-SW", "ok",
          "Kernel recortado, init propio que solo monta la SD y relanza hps_player; sin login.",
          [L("sw/hps/linux/init", None, "init"),
           L("sw/hps/linux/kernel.fragment", None, "kernel.fragment"),
           L("sw/hps/linux/de1_soc_audio.dts", None, "Device tree"),
           L("scripts/build-hps-linux.sh", None, "build-hps-linux.sh")]),
    ]},
    {"id": "codis", "title": "Co-diseño HW/SW (decisión del grupo)", "cite": "§3-SIS p.5–6", "items": [
        R("Control temporal", "§3-SIS", "ok",
          "Tiempo de canción por cuadros de audio (CD-10); reloj en ms con mtime del Nios V.",
          [L(APP, r"^static void count_frame", "count_frame()"),
           L("sw/niosv/drivers/timebase.c", None, "timebase.c (mtime)"),
           L("sw/common/src/clock.c", None, "clock.c")]),
        R("Control de reproducción como FSM", "§3-SIS", "ok", "FSM en software (CD-11).",
          [L("sw/common/src/fsm.c", r"^static const transition_t table", "Tabla de transiciones")]),
        R("Partición HW/SW justificada", "§3 / §5 Metodología", "ok",
          "Decisiones CD-01..CD-14 y comparación de tres arquitecturas completas (CD-15) en el "
          "capítulo 3 del documento de diseño.",
          [L("sim/questa/tb_ciclos.sv", None, "tb_ciclos (medición que sustenta CD-04/05/12)")],
          docs=[L("docs/documento-diseno.md", r"^## 3\. Metodología", "Documento de diseño, cap. 3", 1)] +
               [L(f"docs/decisiones/{p}", None, p.replace(".md", "")) for p in
                ["CD-01.md", "CD-02.md", "CD-04.md", "CD-05.md", "CD-06.md", "CD-07.md", "CD-08.md",
                 "CD-09.md", "CD-10.md", "CD-11.md", "CD-12.md", "CD-13.md", "CD-14.md"]]),
    ]},
    {"id": "rec", "title": "Análisis de recursos y consumo", "cite": "§3 p.6", "items": [
        R("Recursos del sistema y del módulo propio; potencia; discusión", "§3 · Análisis", "ok",
          "Reporte con ALMs, registros, M10K, DSP (sistema y módulo) y PowerPlay.",
          [L("scripts/report-resources.py", None, "report-resources.py"),
           L("scripts/report-power.sh", None, "report-power.sh")],
          docs=[L("docs/metricas/recursos-y-potencia.md", None, "docs/metricas/recursos-y-potencia.md")]),
    ]},
    {"id": "entreg", "title": "Entregables de documentación y repositorio", "cite": "§5 p.7–8", "items": [
        R("Documento de diseño de software + checklist", "§5 (3 %)", "ok", "",
          [], docs=[L("docs/documento-diseno.md", r"^### 6\.11", "Documento de diseño: lista de chequeo", 1),
                    L("docs/software/diseno-software.md", None, "docs/software/diseno-software.md")]),
        R("Presentación funcional", "§5 (60 %)", "ok", "Guion de la demostración, plan B y preguntas probables.",
          [], docs=[L("docs/presentacion.md", None, "docs/presentacion.md")]),
        R("Diseño del hardware personalizado", "§5 (4 %)", "ok", "",
          [], docs=[L("docs/hw/modulo-filtro.md", None, "modulo-filtro.md"),
                    L("docs/hw/seg7-time.md", None, "seg7-time.md"),
                    L("docs/hw/mapa-memoria.md", None, "mapa-memoria.md")]),
        R("Herramientas de ingeniería (scripts, modelos)", "§5 (3 %)", "ok", "",
          [L("Makefile", None, "Makefile"), L("docker/Dockerfile", None, "Dockerfile")],
          docs=[L("docs/herramientas.md", None, "docs/herramientas.md")]),
        R("Flujo Git, CI, README completo", "§5 (20 %)", "parcial",
          "README con diagramas, mapas, resultados y uso; CI obligatoria en main y develop. "
          "Faltan los videos demostrativos (DOC-08).",
          [L(".github/workflows/ci.yml", None, "ci.yml")],
          docs=[L("README.md", None, "README.md"), L("CONTRIBUTING.md", None, "CONTRIBUTING.md"),
                L("TODO.md", None, "TODO.md")]),
    ]},
]

# Bloques del diagrama de arquitectura: (id, titulo, subtitulo, archivo, patron, n)
ARCH = [
    ("hps", "HPS · ARM Cortex-A9", "SD · FAT32 · WAV · remuestreo", "sw/hps/src/producer.c", None, 0),
    ("shm", "Memoria compartida 64 KB", "ring + tabla de pistas", "sw/common/include/shm_proto.h", None, 0),
    ("cpu", "Nios V/m", "main.c → app_step()", "sw/niosv/src/main.c", r"^int main", 0),
    ("ram", "RAM on-chip 128 KB", "código y datos del Nios V", TCL, r"^add_instance ram", 10),
    ("pio", "PIO KEY / SW / LEDR", "IRQ de botones", "sw/niosv/drivers/keys.c", None, 0),
    ("filtro", "filtro_audio (propio)", "FIR 127 taps · ganancia", FA, None, 0),
    ("seg7", "seg7_time (propio)", "MM:SS en HEX", S7, None, 0),
    ("audio", "IP de Audio + config", "FIFO → WM8731", "sw/niosv/drivers/audio.c", None, 0),
    ("adc", "ADC Controller", "potenciómetro → volumen", "sw/niosv/drivers/adc.c", None, 0),
    ("vga", "Character Buffer + VGA", "80 × 60 texto", "sw/niosv/drivers/vga_text.c", None, 0),
]
arch = []
for aid, t, s, f, pat, n in ARCH:
    ref = L(f, pat, t, n)
    if ref:
        arch.append({"id": aid, "t": t, "s": s, "ref": ref})

DESC = {
    "hw/rtl": "HDL propio (filtro y 7 segmentos)",
    "hw/soc": "Definición del SoC en Platform Designer",
    "hw/quartus": "Top-level, pines y tiempos",
    "sw/common": "Lógica portable (probada en el PC)",
    "sw/niosv": "Drivers sin HAL, pruebas y app del Nios V",
    "sw/hps": "Software del ARM y Linux mínimo",
    "sim": "Testbenches Questa y vectores",
    "tests": "Pruebas en el PC (make test)",
    "scripts": "Compilar, simular, programar, reportar",
    "docs": "Decisiones, hardware, verificación",
}

if missing:
    print("Referencias sin resolver:", *missing, sep="\n  ", file=sys.stderr)
    sys.exit(1)

rev = subprocess.check_output(["git", "log", "-1", "--format=%h · %ad · %s", "--date=short"],
                              cwd=ROOT, text=True).strip()
branch = subprocess.check_output(["git", "rev-parse", "--abbrev-ref", "HEAD"], cwd=ROOT, text=True).strip()
payload = {"files": files, "sections": SECTIONS, "arch": arch, "desc": DESC,
           "rev": rev, "branch": branch}

tpl = (Path(__file__).with_name("mapa-codigo.template.html")).read_text(encoding="utf-8")
blob = json.dumps(payload, ensure_ascii=False).replace("</", "<\\/")
OUT.write_text(tpl.replace("/*__DATA__*/null", blob), encoding="utf-8")
nreq = sum(len(s["items"]) for s in SECTIONS)
print(f"{OUT.relative_to(ROOT)}: {len(files)} archivos, {nreq} requisitos, {OUT.stat().st_size // 1024} KB")
