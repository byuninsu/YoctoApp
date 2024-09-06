SUMMARY = "discrete in Application"
DESCRIPTION = "discrete in application for Yocto"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"
PACKAGE_NAME = "libdiscrete-in"

PN = "libdiscrete-in"

SRC_URI = "file://libdiscrete_in.c \
           file://libdiscrete_in.h"
           
S = "${WORKDIR}"

do_compile() {
    ${CC} ${CFLAGS} -fPIC -c libdiscrete_in.c -o libdiscrete_in.o
    ${CC} ${LDFLAGS} -shared -Wl,--hash-style=gnu -o libdiscrete-in.so.1.0 libdiscrete_in.o
    ln -sf libdiscrete-in.so.1.0 libdiscrete-in.so
}

do_install() {
    install -d ${D}${includedir}
    install -m 0644 libdiscrete_in.h ${D}${includedir}

    install -d ${D}${libdir}
    install -m 0755 libdiscrete-in.so.1.0 ${D}${libdir}
    ln -sf libdiscrete-in.so.1.0 ${D}${libdir}/libdiscrete-in.so
}

FILES_${PN} = "${libdir}/libdiscrete-in.so.1.0 ${libdir}/libdiscrete-in.so"
FILES_${PN}-dev += "${includedir}/libdiscrete_in.h"
