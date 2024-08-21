SUMMARY = "bit-control Application"
DESCRIPTION = "bit-control application for Yocto"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"
PACKAGE_NAME = "bit-control"

PN = "bit-control"
DEPENDS += "cjson"

CFLAGS += "-I${STAGING_INCDIR}"


SRC_URI = "file://bit_control.c \
           file://bit_control.h"
S = "${WORKDIR}"

do_compile() {
    ${CC} ${CFLAGS} ${LDFLAGS} -I${STAGING_INCDIR}/cjson -o bit-control bit_control.c -lcjson
}


do_install() {
    install -d ${D}${bindir}
    install -m 0755 bit-control ${D}${bindir}
}

FILES_${PN} = "${bindir}/bit-control"

