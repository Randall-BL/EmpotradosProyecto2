# Guía de contribución

Convenciones acordadas por el grupo para el Proyecto II del curso CE-1113.
Aplican a **todo** el historial del repositorio: el flujo de trabajo Git es un rubro
evaluado (10%).

---

## 1. Modelo de ramas

Se usa **Git Flow simplificado con rama de integración** (`develop`):

| Rama | Propósito |
|---|---|
| `main` | Solo código estable y presentable. **Protegida**: se actualiza únicamente vía Pull Request. |
| `develop` | Rama de integración. Aquí convergen todas las funcionalidades antes de subir a `main`. |
| `YitanDevelop` | Rama personal de desarrollo de Randall; se integra a `develop` por Pull Request. |
| `feat/<tema>` | Una rama por issue. Sale de `develop` y vuelve a `develop` por Pull Request. |
| `fix/<tema>` | Corrección de un bug reportado como issue. |
| `docs/<tema>` | Cambios exclusivamente de documentación. |

Nombres de rama en minúscula y con guiones: `feat/filtro-fir-hdl`,
`fix/fifo-audio-underrun`, `docs/mapa-memoria`.

```bash
git switch develop && git pull
git switch -c feat/filtro-fir-hdl
# ... trabajo y commits ...
git push -u origin feat/filtro-fir-hdl
gh pr create --base develop --title "feat(dsp): filtro FIR pasa-bajos en HDL" --body "Refs #12"
```

### Protección de `main`

`main` tiene activada la regla de protección de rama:

- Prohibido `git push` directo — todo cambio entra por Pull Request.
- Prohibido el force-push y el borrado de la rama.
- Las conversaciones del PR deben resolverse antes de hacer merge.

Si un `git push origin main` es rechazado, **no es un error**: es la protección
funcionando. Abra un PR desde `develop`.

---

## 2. Conventional Commits

Formato obligatorio del mensaje de commit:

```
<tipo>(<ámbito opcional>): <descripción en imperativo, minúscula, sin punto final>

<cuerpo opcional: el porqué del cambio, no el qué>

Refs #<issue>
```

### Tipos

| Tipo | Cuándo usarlo |
|---|---|
| `feat` | Nueva funcionalidad |
| `fix` | Corrección de un defecto |
| `docs` | Solo documentación |
| `chore` | Tareas de mantenimiento, estructura, configuración |
| `refactor` | Reestructuración sin cambio de comportamiento |
| `test` | Testbenches y rutinas de prueba |
| `perf` | Mejoras de rendimiento, latencia o consumo de recursos |
| `build` | Sistema de construcción: Quartus, Platform Designer, BSP, Makefiles/CMake |

### Ámbitos del proyecto

`hw`, `dsp`, `soc`, `sw`, `sd`, `wav`, `irq`, `ui`, `audio`, `adc`, `display`,
`metrics`, `docs`.

### Ejemplos

```
feat(hw): agrega registros de control y estado al modulo de filtro
test(dsp): agrega testbench del FIR con respuesta al impulso
feat(irq): atiende los botones KEY por interrupcion
build(soc): agrega el IP ADC Controller al sistema en Platform Designer
docs(metrics): reporta utilizacion de ALMs y potencia con PowerPlay
```

### Reglas

- Un commit = un cambio con sentido propio. Nada de `varios cambios` ni `wip`.
- Descripción de 72 caracteres o menos, en imperativo (`agrega`, no `agregado`).
- Historial **distribuido en el tiempo**: se evalúa que no exista un único commit final.
- Sin acentos ni caracteres especiales en el asunto del commit (compatibilidad de consola).

---

## 3. Vinculación con issues

Todo trabajo nace de un issue. La trazabilidad commit ↔ PR ↔ issue es un rubro evaluado.

- En los **commits**: `Refs #12` al final del cuerpo.
- En los **Pull Requests** hacia `develop`: `Refs #12` (no cierra el issue todavía).
- En el **Pull Request final hacia `main`**: `Closes #12` — GitHub cierra el issue
  automáticamente al hacer merge, porque `main` es la rama por defecto.

Un issue solo se cierra cuando la funcionalidad está **probada en la DE1-SoC**, no cuando
el código está escrito.

### Etiquetas

Etiquetas de área (`area:git`, `area:hw`, `area:dsp`, `area:soc`, `area:sw`,
`area:audio`, `area:display`, `area:metricas`, `area:docs`) y de carácter
(`OBL` obligatorio, `OPC` opcional / crédito extra, `EVID` genera evidencia entregable).

### Board

Todos los issues están en el proyecto **Reproductor de Audio - Proyecto II** con estado
`Todo` → `In Progress` → `Done`.

---

## 4. Pull Requests

- Base `develop` para las funcionalidades; base `main` solo para integrar.
- Título con el mismo formato Conventional Commit del cambio principal.
- El cuerpo indica **qué se probó y cómo** (simulación, rutina SW, tarjeta); si no se
  pudo probar en la DE1-SoC, se dice.
- Al menos una persona del grupo revisa antes del merge.
- Merge con **squash** cuando la rama tiene commits de arreglo intermedios; merge normal
  cuando el historial de la rama ya es limpio.

---

## 5. Convenciones de código

### Software bare metal (C)

- **Sin HAL.** Nada de funciones `alt_*` ni macros `IOWR`/`IORD`: los periféricos se
  acceden con punteros `volatile` a las direcciones base de `system.h`.
  **Única excepción:** el registro y habilitación de interrupciones
  (`alt_ic_isr_register`, `alt_ic_irq_enable`).
- Estándar **C11**, compilado con `-Wall -Wextra`.
- Indentación de **4 espacios**, nunca tabuladores. Llave de apertura en la misma línea.
- Nombres en `snake_case`; constantes, macros y offsets de registro en `MAYUSCULAS`.
- Prefijo de módulo en toda función pública: `sd_read_block()`, `wav_parse_header()`,
  `audio_write_sample()`, `seg7_show_time()`.
- Las funciones internas del módulo son `static`. Un archivo por módulo, con su header y
  guarda de inclusión.
- Las ISR son cortas: limpian la fuente, registran el evento y salen.
- Comentarios en español, explicando el **porqué**.

### HDL

- Un módulo por archivo, nombre de archivo = nombre del módulo.
- Reset síncrono y explícito; nada de latches inferidos.
- Mapa de registros documentado en el encabezado del módulo y en `docs/`.
- Todo módulo propio tiene su testbench en `hw/tb/`.

---

## 6. Antes de abrir un PR

- [ ] El diseño compila en Quartus y el software compila sin warnings nuevos.
- [ ] `grep -rnE 'IOWR|IORD|alt_' software/` solo muestra llamadas de interrupciones.
- [ ] Los commits siguen Conventional Commits y referencian su issue.
- [ ] La rama está al día con `develop`.
- [ ] Nada de `db/`, `output_files/`, `.sof`, BSP generado ni binarios en el diff.
- [ ] La documentación afectada quedó actualizada en el mismo PR.
