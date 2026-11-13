# Imagen para construir el arranque del HPS (HP-02, HP-03): U-Boot (SPL), kernel Linux,
# BusyBox y la aplicacion del HPS. Parte de la misma base fijada que docker/Dockerfile
# y es independiente de ella, para no engordar la imagen de la CI.
#
# Los paquetes no llevan version fija: las versiones exactas con que se construyo la
# imagen quedan en docker/versions.md (las fijas de docker/Dockerfile vencen cuando
# Ubuntu publica una actualizacion; ver el hallazgo de DK-01 en el TODO).
#
# Construir:  docker compose -f docker/compose.yml build hps
# Usar:       scripts/build-hps-linux.sh (se ejecuta dentro de este contenedor)
FROM ubuntu:24.04@sha256:fe81560990fae86dc4743202b2cb4d3df98d0bcf949173cecef15ce1127ef852

ARG DEBIAN_FRONTEND=noninteractive
ARG USER_NAME=dev
ARG USER_UID=1000
ARG USER_GID=1000

RUN apt-get update && apt-get install -y --no-install-recommends \
        ca-certificates make gcc libc6-dev file \
        gcc-arm-linux-gnueabihf libc6-dev-armhf-cross \
        bc bison flex cpio kmod rsync xz-utils bzip2 curl \
        libssl-dev libgnutls28-dev uuid-dev \
        python3 python3-dev python3-setuptools python3-pyelftools swig \
        fdisk dosfstools mtools u-boot-tools \
    && rm -rf /var/lib/apt/lists/*

RUN (userdel -r ubuntu 2>/dev/null || true) \
    && (getent group "${USER_GID}" >/dev/null || groupadd -g "${USER_GID}" "${USER_NAME}") \
    && useradd -m -u "${USER_UID}" -g "${USER_GID}" -s /bin/bash "${USER_NAME}"
USER ${USER_NAME}
WORKDIR /work
CMD ["bash"]
