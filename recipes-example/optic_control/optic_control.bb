SUMMARY = "optic_control Application"
DESCRIPTION = "optic_control application for Yocto"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

PACKAGE_NAME = "optic-control"

PN = "optic-control"

SRC_URI = "file://optic_control.c \
           file://optic_control.h"
S = "${WORKDIR}"

do_compile() {
    ${CC} ${CFLAGS} -c optic_control.c -o optic_control.o
    ar rcs liboptic-control.a optic_control.o
}

do_install() {
    install -d ${D}${includedir}
    install -m 0644 optic_control.h ${D}${includedir}

    install -d ${D}${libdir}
    install -m 0644 liboptic-control.a ${D}${libdir}
}

