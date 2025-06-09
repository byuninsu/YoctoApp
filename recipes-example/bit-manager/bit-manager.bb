SUMMARY = "bit-manager Application"
DESCRIPTION = "bit-manager application for Yocto"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

PN = "bit-manager"

DEPENDS += "usb-control"
DEPENDS += "stm32-control"
DEPENDS += "rs232-control"
DEPENDS += "cjson"
DEPENDS += "push-button"
DEPENDS += "nvram-control"
DEPENDS += "led-control"
DEPENDS += "ethernet-control"
DEPENDS += "discrete-in"
DEPENDS += "optic-control"

SRC_URI = "file://bit_manager.c \
           file://bit_manager.h \
           file://mii.h"

S = "${WORKDIR}"

do_compile() {
    ${CC} ${CFLAGS} ${LDFLAGS} \
    -I${STAGING_INCDIR}/usb-control \
    -I${STAGING_INCDIR}/cjson \
    -I${STAGING_INCDIR}/stm32-control \
    -I${STAGING_INCDIR}/rs232-control \
    -I${STAGING_INCDIR}/nvram-control \
    -I${STAGING_INCDIR}/led-control \
    -I${STAGING_INCDIR}/ethernet-control \
    -I${STAGING_INCDIR}/discrete-in \
    -I${STAGING_INCDIR}/optic-control \
    -c bit_manager.c -o bit_manager.o

    ar rcs libbit-manager.a bit_manager.o 
}


do_install() {
    install -d ${D}${libdir}
    install -d ${D}${includedir}
    install -m 0755 libbit-manager.a ${D}${libdir}
    install -m 0644 bit_manager.h ${D}${includedir}
}

FILES_${PN} = "${libdir}/libbit-manager.a ${includedir}/bit_manager.h"

