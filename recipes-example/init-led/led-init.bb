SUMMARY = "led init script"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

PN = "led-init"

DEPENDS += "led-control"
DEPENDS += "stm32-control"
DEPENDS += "nvram-control"
DEPENDS += "cjson"

SRC_URI = "file://led_init.c"

# Compilation
do_compile() {
    ${CC} ${CFLAGS} ${LDFLAGS} \
        -I${STAGING_INCDIR}/led-control \
        -I${STAGING_INCDIR}/stm32-control \
        -I${STAGING_INCDIR}/nvram-control \
        -I${STAGING_INCDIR}/cjson \
        -L${STAGING_LIBDIR} \
        ${WORKDIR}/led_init.c \
        -o led_init \
        -lled-control \
        -lstm32-control \
        -lnvram-control \
        -lcjson
}

# Installation
do_install() {
    install -d ${D}${bindir}
    install -m 0755 ${B}/led_init ${D}${bindir}/led-init
}

INITSCRIPT_NAME = "led_init"
INITSCRIPT_PARAMS = "defaults"

