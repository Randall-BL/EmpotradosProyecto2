# Contenedor de desarrollo

Entorno reproducible para todo lo que **no** necesita Quartus: pruebas en C de
`sw/common`, simulación del HDL propio con cocotb + Icarus, lint con Verilator,
compilación para el ARM del HPS, herramientas para la imagen de la SD y la CI.
Versiones exactas: [versions.md](versions.md).

## Uso

```bash
make docker-build      # una vez (o cuando cambie docker/Dockerfile)
make docker-ci         # lint + test + sim + hps, igual que la CI
make docker-test       # cualquier objetivo del Makefile raíz: docker-<objetivo>
make docker-shell      # terminal en /work (el repositorio montado)
```

Sin `make`: `docker compose -f docker/compose.yml run --rm dev make test`.

El repositorio se monta como volumen en `/work` (no se copia a la imagen) y el usuario
del contenedor tiene el UID/GID del anfitrión (`HOST_UID`/`HOST_GID`, que exporta el
Makefile raíz), así que lo que se genera en `build/` es del usuario y no de root (DK-03).

## Qué se hace dentro y qué fuera

| Dentro del contenedor | Fuera (nativo, [docs/entorno-nativo.md](../docs/entorno-nativo.md)) |
|---|---|
| `make test`: pruebas en C de `sw/common` con sanitizers | Platform Designer (`scripts/build-soc.sh`) |
| `make sim`: cocotb + Icarus de los módulos propios | Quartus: síntesis, fitter, tiempos, potencia |
| `make lint`: Verilator + auditoría sin HAL | BSP y compilación del Nios V (`scripts/build-niosv.sh`) |
| `make hps`: toolchain ARM del HPS | Questa: co-simulación del SoC (`scripts/test-all.sh`) |
| Imagen de la SD (`sfdisk`, `mkfs.fat`, `mtools`, `mkimage`) | Programar la tarjeta por JTAG (`scripts/program.sh`) |
| CI en GitHub Actions | |

Por qué Quartus queda fuera:

- **JTAG/USB-Blaster:** programar la tarjeta y depurar necesita el dispositivo USB y el
  servidor `jtagd` del anfitrión.
- **Herramientas gráficas:** Platform Designer, el Pin Planner y la GUI de Questa.
- **Tamaño:** Quartus Lite 25.1 + Questa ocupan más de 20 GB; la imagen tiene un límite
  de 1 GB (DK-07).
- **Licencia de Questa:** la licencia Starter está atada a la MAC del equipo; dentro de
  un contenedor la MAC cambia.

## Decisiones

- **Base (DK-01):** `ubuntu:24.04` fijada por digest. Frente a `debian:bookworm-slim`:
  trae Verilator 5 e Icarus 12 en sus repositorios (bookworm tiene Verilator 5.006) y es
  la misma versión del runner de GitHub Actions (`ubuntu-24.04`), así que la CI y el
  contenedor se comportan igual.
- **Una capa por grupo de herramientas (DK-04)**, cada una borra la caché de `apt`
  (`/var/lib/apt/lists`) en el mismo `RUN`; `pip` con `--no-cache-dir`. Cambiar las
  herramientas de Python no reconstruye las de C.
- **Una sola toolchain ARM:** ver [versions.md](versions.md).

## Tamaño (DK-07)

Medido el 2026-11-04 con `docker image ls` (Docker 29, almacén de containerd):

| Versión del Dockerfile | `DISK USAGE` | `CONTENT SIZE` (comprimida) | Sistema de archivos |
|---|---|---|---|
| Con `gcc-arm-none-eabi` + newlib, `cmake`, `git` | 2.03 GB | 476 MB | — |
| Solo `arm-linux-gnueabihf` | 1.23 GB | 313 MB | — |
| Sin `cmake`/`git` | 1.06 GB | 267 MB | 755 MB |
| **Actual:** sin documentación, fuentes ni ejemplos; `.so` de Python sin símbolos | **999 MB** | **252 MB** | **704 MB** |

`DISK USAGE` suma la imagen descomprimida y la comprimida que guarda containerd; lo que
se descarga es `CONTENT SIZE`. El margen bajo 1 GB es poco: antes de agregar una
herramienta, medir con `make docker-build && docker image ls empotrados-dev`.

## CI

[`.github/workflows/ci.yml`](../.github/workflows/ci.yml) construye esta misma imagen
(con caché de capas de GitHub Actions) en cada `push` y `pull_request` hacia `develop`
y `main`, y corre `make lint`, `make test`, `make sim` y `make hps` dentro de ella. Los
logs y, cuando haya, los `results.xml` y las formas de onda de cocotb se suben como
artefactos durante 14 días, también si algo falla (DK-18).

### Hacer obligatorio el check (DK-19)

Lo configura quien administra el repositorio en GitHub: *Settings → Branches →* regla
de `main` y de `develop` → *Require status checks to pass before merging* → buscar y
marcar **`lint, test, sim, hps`** (el nombre del job). Para que aparezca en la lista, el
workflow tiene que haber corrido al menos una vez en un PR.
