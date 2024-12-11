SUMMARY = "Optic Control Library"
DESCRIPTION = "Optic Control library for Yocto"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

PN = "liboptic-control"

DEPENDS += "ethernet-control"

SRC_URI = "file://liboptic_control.c \
           file://liboptic_control.h"
           
S = "${WORKDIR}"

do_compile() {
    ${CC} ${CFLAGS} -I${STAGING_INCDIR}/ethernet-control -fPIC -c liboptic_control.c -o liboptic_control.o
    ${CC} ${LDFLAGS} -shared -Wl,--hash-style=gnu -o liboptic-control.so.1.0 liboptic_control.o
    ln -sf liboptic-control.so.1.0 liboptic-control.so
}

do_install() {
    install -d ${D}${includedir}
    install -m 0644 liboptic_control.h ${D}${includedir}

    install -d ${D}${libdir}
    install -m 0755 liboptic-control.so.1.0 ${D}${libdir}
    ln -sf liboptic-control.so.1.0 ${D}${libdir}/liboptic-control.so
}

FILES_${PN} = "${libdir}/liboptic-control.so.1.0 ${libdir}/liboptic-control.so"
FILES_${PN}-dev += "${includedir}/liboptic_control.h"

