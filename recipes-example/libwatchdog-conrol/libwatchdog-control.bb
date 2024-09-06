SUMMARY = "Watchdog Control Application"
DESCRIPTION = "Watchdog Control application for Yocto"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

PN = "libwatchdog-control"

SRC_URI = "file://libwatchdog_control.c \
           file://libwatchdog_control.h"
           
S = "${WORKDIR}"

do_compile() {
    ${CC} ${CFLAGS} -fPIC -c libwatchdog_control.c -o libwatchdog_control.o
    ${CC} ${LDFLAGS} -shared -Wl,--hash-style=gnu -o libwatchdog-control.so.1.0 libwatchdog_control.o
    ln -sf libwatchdog-control.so.1.0 libwatchdog-control.so
}

do_install() {
    install -d ${D}${includedir}
    install -m 0644 libwatchdog_control.h ${D}${includedir}

    install -d ${D}${libdir}
    install -m 0755 libwatchdog-control.so.1.0 ${D}${libdir}
    ln -sf libwatchdog-control.so.1.0 ${D}${libdir}/libwatchdog-control.so
}

FILES_${PN} = "${libdir}/libwatchdog-control.so.1.0 ${libdir}/libwatchdog-control.so"
FILES_${PN}-dev += "${includedir}/libwatchdog_control.h"


