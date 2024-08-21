SUMMARY = "push-button Application"
DESCRIPTION = "push-button application for Yocto"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"
PACKAGE_NAME = "push-button"

PN = "push-button"

SRC_URI = "file://push_button.c \
           file://push_button.h"
S = "${WORKDIR}"

do_compile() {
    ${CC} ${CFLAGS} -c push_button.c -o push_button.o
    ar rcs libpush-button.a push_button.o
}

do_install() {
    install -d ${D}${includedir}
    install -m 0644 push_button.h ${D}${includedir}

    install -d ${D}${libdir}
    install -m 0644 libpush-button.a ${D}${libdir}
}


