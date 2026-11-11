# Contenedor de desarrollo del proyecto (seccion 8 del TODO, DK-01..DK-07).
#
# Sirve para: pruebas en C de sw/common (make test), simulacion cocotb + Icarus del HDL
# propio (make sim), lint con Verilator (make lint), toolchain ARM del HPS (make hps),
# herramientas para la imagen de la SD y la CI. NO reemplaza a Quartus, Platform
# Designer, el BSP del Nios V ni Questa: esos se usan de forma nativa
# (docs/entorno-nativo.md, docker/README.md).
#
# Versiones fijas: docker/versions.md. Construir con:
#   make docker-build      (o: docker compose -f docker/compose.yml build)

# DK-01: base fijada por digest (ubuntu:24.04 al 2026-11-03).
FROM ubuntu:24.04@sha256:fe81560990fae86dc4743202b2cb4d3df98d0bcf949173cecef15ce1127ef852

ARG DEBIAN_FRONTEND=noninteractive
# DK-03: el usuario del contenedor toma el UID/GID del anfitrion para que lo que se
# genere en /work no quede de root.
ARG USER_NAME=dev
ARG USER_UID=1000
ARG USER_GID=1000

# Capa 1: compilacion nativa (pruebas en C del PC).
RUN apt-get update && apt-get install -y --no-install-recommends \
        ca-certificates=20260816ubuntu0.24.04.2 \
        make=4.3-4.1build2 \
        gcc=4:13.2.0-7ubuntu1 \
        libc6-dev=2.39-0ubuntu8.9 \
        file=1:5.45-3build1 \
    && rm -rf /var/lib/apt/lists/*

# Capa 2: HDL (Icarus para cocotb, Verilator para lint), segun CD-13 (Verilog-2005).
RUN apt-get update && apt-get install -y --no-install-recommends \
        iverilog=12.0-2build2 \
        verilator=5.020-1 \
    && rm -rf /var/lib/apt/lists/* \
    && rm -rf /usr/share/doc/* /usr/share/man/* /usr/share/fonts /usr/share/javascript \
              /usr/share/sphinx_rtd_theme /usr/share/verilator/examples

# Capa 3: toolchain ARM del HPS. Una sola (DK-07): con gcc-arm-none-eabi + newlib la
# imagen pasaba de 2 GB. arm-linux-gnueabihf sirve para las dos opciones de CD-02:
# compila U-Boot/SPL (es la que usa la guia de U-Boot para socfpga), la aplicacion de
# Linux y, con -ffreestanding -nostdlib, una aplicacion bare metal.
RUN apt-get update && apt-get install -y --no-install-recommends \
        gcc-arm-linux-gnueabihf=4:13.2.0-7ubuntu1 \
        libc6-dev-armhf-cross=2.39-0ubuntu8cross1 \
    && rm -rf /var/lib/apt/lists/*

# Capa 4: imagen de la SD (HP-03): particiones, FAT32 y mkimage de U-Boot.
RUN apt-get update && apt-get install -y --no-install-recommends \
        fdisk=2.39.3-9ubuntu6.6 \
        dosfstools=4.2-1.1build1 \
        mtools=4.0.43-1build1 \
        u-boot-tools=2025.10-0ubuntu0.24.04.4 \
    && rm -rf /var/lib/apt/lists/*

# Capa 5: Python (cocotb y modelos de referencia) en un venv: Ubuntu 24.04 no deja
# instalar con pip sobre el Python del sistema (PEP 668).
RUN apt-get update && apt-get install -y --no-install-recommends \
        python3=3.12.3-0ubuntu2.1 \
        python3-venv=3.12.3-0ubuntu2.1 \
    && rm -rf /var/lib/apt/lists/* \
    && python3 -m venv /opt/venv \
    && /opt/venv/bin/pip install --no-cache-dir \
        cocotb==1.9.2 \
        numpy==2.1.3 \
        scipy==1.14.1 \
        pytest==8.3.4 \
    && /opt/venv/bin/pip uninstall -y pip \
    && find /opt/venv -name '*.so' -exec strip --strip-unneeded {} + \
    && find /opt/venv -name '__pycache__' -prune -exec rm -rf {} + \
    && rm -rf /opt/venv/lib/python3.12/site-packages/scipy/*/tests \
              /opt/venv/lib/python3.12/site-packages/numpy/*/tests
ENV PATH=/opt/venv/bin:$PATH \
    PYTHONDONTWRITEBYTECODE=1

# Usuario no root. ubuntu:24.04 trae el usuario "ubuntu" con UID 1000; se quita
# para poder usar el UID del anfitrion.
RUN (userdel -r ubuntu 2>/dev/null || true) \
    && (getent group "${USER_GID}" >/dev/null || groupadd -g "${USER_GID}" "${USER_NAME}") \
    && useradd -m -u "${USER_UID}" -g "${USER_GID}" -s /bin/bash "${USER_NAME}"
USER ${USER_NAME}
WORKDIR /work
CMD ["bash"]
