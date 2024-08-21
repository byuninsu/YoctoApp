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

SRC_URI = "file://bit_manager.c \
           file://bit_manager.h"

S = "${WORKDIR}"

do_compile() {
     ${CC} ${CFLAGS} ${LDFLAGS} \
     -I${STAGING_INCDIR}/usb-control\ 
     -I${STAGING_INCDIR}/cjson \
     -I${STAGING_INCDIR}/stm32-control \
     -I${STAGING_INCDIR}/rs232-control \
     -I${STAGING_INCDIR}/nvram-control \
     -I${STAGING_INCDIR}/led-control \
     -I${STAGING_INCDIR}/ethernet-control \
     -I${STAGING_INCDIR}/discrete-in \
     -o bit-manager bit_manager.c \
     -lusb-control \
     -lstm32-control \
     -lcjson \
     -lrs232-control \
     -lnvram-control \
     -lled-control \
     -lethernet-control \
     -ldiscrete-in
}


do_install() {
    install -d ${D}${bindir}
    install -m 0755 bit-manager ${D}${bindir}
}

FILES_${PN} = "${bindir}/bit-manager"

