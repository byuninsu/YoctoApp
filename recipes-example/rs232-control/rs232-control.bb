SUMMARY = "rs232 Contrl Application"
DESCRIPTION = "rs232 Contrl application for Yocto"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"
PACKAGE_NAME = "rs232-control"

PN = "rs232-control"

SRC_URI = "file://rs232_control.c \
           file://rs232_control.h"
           
S = "${WORKDIR}"

do_compile() {
    ${CC} ${CFLAGS} -c rs232_control.c -o rs232_control.o
    ar rcs librs232-control.a rs232_control.o
}


do_install() {
    install -d ${D}${includedir}
    install -m 0644 rs232_control.h ${D}${includedir}

    install -d ${D}${libdir}
    install -m 0644 librs232-control.a ${D}${libdir}
}



