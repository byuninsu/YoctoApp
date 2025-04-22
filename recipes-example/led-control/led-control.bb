SUMMARY = "LED Contrl Application"
DESCRIPTION = "LED Contrl application for Yocto"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"
PACKAGE_NAME = "led-control"

DEPENDS += "stm32-control"

PN = "led-control"

SRC_URI = "file://led_control.c \
           file://led_control.h"
S = "${WORKDIR}"

do_compile() {
    ${CC} ${CFLAGS} \
        -I${STAGING_INCDIR} \
        -I${STAGING_INCDIR}/stm32-control \
        -L${STAGING_LIBDIR} \
        -lstm32-control \
        -c led_control.c -o led_control.o

    ar rcs libled-control.a led_control.o
}

do_install() {
    install -d ${D}${includedir}
    install -m 0644 led_control.h ${D}${includedir}

    install -d ${D}${libdir}
    install -m 0644 libled-control.a ${D}${libdir}
}




