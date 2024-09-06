SUMMARY = "watchdog-control Application"
DESCRIPTION = "watchdog-control application for Yocto"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"
PACKAGE_NAME = "watchdog-control"

PN = "watchdog-control"

SRC_URI = "file://watchdog_control.c \
           file://watchdog_control.h"
           
S = "${WORKDIR}"


do_compile() {
    ${CC} ${CFLAGS} -c watchdog_control.c -o watchdog_control.o
    ar rcs libwatchdog-control.a watchdog_control.o
}


do_install() {
    install -d ${D}${includedir}
    install -m 0644 watchdog_control.h ${D}${includedir}

    install -d ${D}${libdir}
    install -m 0644 libwatchdog-control.a ${D}${libdir}
}


