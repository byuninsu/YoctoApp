SUMMARY = "ethernet-control Application"
DESCRIPTION = "ethernet-control application for Yocto"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"
PACKAGE_NAME = "ethernet-control"

PN = "ethernet-control"

SRC_URI = "file://ethernet_control.c \
           file://ethernet_control.h \
           file://mii.h"
S = "${WORKDIR}"

do_compile() {
    ${CC} ${CFLAGS} -c ethernet_control.c -o ethernet_control.o
    ar rcs libethernet-control.a ethernet_control.o
}

do_install() {
    install -d ${D}${includedir}
    install -m 0644 ethernet_control.h ${D}${includedir}

    install -d ${D}${libdir}
    install -m 0644 libethernet-control.a ${D}${libdir}
}

