SUMMARY = "nvram Contrl Application"
DESCRIPTION = "nvram Contrl application for Yocto"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"
PACKAGE_NAME = "nvram-control"

PN = "nvram-control"

SRC_URI = "file://nvram_control.c \
           file://nvram_control.h"
           
S = "${WORKDIR}"

do_compile() {
    ${CC} ${CFLAGS} -c nvram_control.c -o nvram_control.o
    ar rcs libnvram-control.a nvram_control.o
}

do_install() {
    install -d ${D}${includedir}
    install -m 0644 nvram_control.h ${D}${includedir}

    install -d ${D}${libdir}
    install -m 0644 libnvram-control.a ${D}${libdir}
}





