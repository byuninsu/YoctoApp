SUMMARY = "libbit-manager Application"
DESCRIPTION = "libbit-manager application for Yocto"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

PN = "libbit-manager"

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

SRC_URI = "file://libbit_manager.c \
           file://libbit_manager.h \
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
    -fPIC -c libbit_manager.c -o libbit_manager.o

    ${CC} ${LDFLAGS} -shared -Wl,--hash-style=gnu -o libbit-manager.so.1.0 libbit_manager.o \
    -L${STAGING_LIBDIR} -lusb-control -lcjson -lstm32-control -lrs232-control -lnvram-control \
    -lled-control -lethernet-control -ldiscrete-in -loptic-control

    ln -sf libbit-manager.so.1.0 libbit-manager.so
}


do_install() {
    install -d ${D}${libdir}
    install -d ${D}${includedir}
    install -m 0755 libbit-manager.so.1.0 ${D}${libdir}
    ln -sf libbit-manager.so.1.0 ${D}${libdir}/libbit-manager.so
    install -m 0644 libbit_manager.h ${D}${includedir}
}

FILES_${PN} = "${libdir}/libbit-manager.so.1.0 ${libdir}/libbit-manager.so ${includedir}/libbit_manager.h"
FILES_${PN}-dev += "${includedir}/libbit_manager.h"

