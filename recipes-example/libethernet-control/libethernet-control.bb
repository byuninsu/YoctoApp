SUMMARY = "Ethernet Control Application"
DESCRIPTION = "Ethernet Control application for Yocto"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"
PACKAGE_NAME = "libethernet-control"

PN = "libethernet-control"

SRC_URI = "file://libethernet_control.c \
           file://libethernet_control.h \
           file://mii.h"
           
S = "${WORKDIR}"

do_compile() {
    ${CC} ${CFLAGS} -fPIC -c libethernet_control.c -o libethernet_control.o
    ${CC} ${LDFLAGS} -shared -Wl,--hash-style=gnu -o libethernet-control.so.1.0 libethernet_control.o
    ln -sf libethernet-control.so.1.0 libethernet-control.so
}

do_install() {
    install -d ${D}${includedir}
    install -m 0644 libethernet_control.h ${D}${includedir}
    install -m 0644 mii.h ${D}${includedir}

    install -d ${D}${libdir}
    install -m 0755 libethernet-control.so.1.0 ${D}${libdir}
    ln -sf libethernet-control.so.1.0 ${D}${libdir}/libethernet-control.so
}

FILES_${PN} = "${libdir}/libethernet-control.so.1.0 ${libdir}/libethernet-control.so"
FILES_${PN}-dev += "${includedir}/libethernet_control.h ${includedir}/mii.h"
