SUMMARY = "discrete in Application"
DESCRIPTION = "discrete in application for Yocto"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"
PACKAGE_NAME = "discrete-in"

PN = "discrete-in"

SRC_URI = "file://discrete_in.c \
           file://discrete_in.h"
           
S = "${WORKDIR}"

do_compile() {
    ${CC} ${CFLAGS} -c discrete_in.c -o discrete_in.o
    ar rcs libdiscrete-in.a discrete_in.o
}

do_install() {
    install -d ${D}${includedir}
    install -m 0644 discrete_in.h ${D}${includedir}

    install -d ${D}${libdir}
    install -m 0644 libdiscrete-in.a ${D}${libdir}
}






