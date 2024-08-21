SUMMARY = "discrete out Application"
DESCRIPTION = "discrete out application for Yocto"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

PACKAGE_NAME = "discrete-out"

PN = "discrete-out"

SRC_URI = "file://discrete_out.c \
           file://discrete_out.h"
S = "${WORKDIR}"

do_compile() {
    ${CC} ${CFLAGS} -c discrete_out.c -o discrete_out.o
    ar rcs libdiscrete-out.a discrete_out.o
}

do_install() {
    install -d ${D}${includedir}
    install -m 0644 discrete_out.h ${D}${includedir}

    install -d ${D}${libdir}
    install -m 0644 libdiscrete-out.a ${D}${libdir}
}

