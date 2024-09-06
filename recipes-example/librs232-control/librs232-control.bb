SUMMARY = "RS232 Control Application"
DESCRIPTION = "RS232 Control application for Yocto"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"
PACKAGE_NAME = "librs232-control"

PN = "librs232-control"

SRC_URI = "file://librs232_control.c \
           file://librs232_control.h"
           
S = "${WORKDIR}"

do_compile() {
    ${CC} ${CFLAGS} -fPIC -c librs232_control.c -o librs232_control.o
    ${CC} ${LDFLAGS} -shared -Wl,--hash-style=gnu -o librs232-control.so.1.0 librs232_control.o
    ln -sf librs232-control.so.1.0 librs232-control.so
}

do_install() {
    install -d ${D}${includedir}
    install -m 0644 librs232_control.h ${D}${includedir}

    install -d ${D}${libdir}
    install -m 0755 librs232-control.so.1.0 ${D}${libdir}
    ln -sf librs232-control.so.1.0 ${D}${libdir}/librs232-control.so
}

FILES_${PN} = "${libdir}/librs232-control.so.1.0 ${libdir}/librs232-control.so"
FILES_${PN}-dev += "${includedir}/librs232_control.h"

