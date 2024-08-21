SUMMARY = "MDIO tool Application"
DESCRIPTION = "MDIO tool application for Yocto"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"
PACKAGE_NAME = "mdio-tool"

PN = "mdio-tool"

SRC_URI = "file://mdio_tool.c \
           file://mii.h"
S = "${WORKDIR}"


do_compile() {
    ${CC} ${CFLAGS} ${LDFLAGS} -o mdio-tool mdio_tool.c
}


do_install() {
    install -d ${D}${bindir}
    install -m 0755 mdio-tool ${D}${bindir}
}

FILES_${PN} = "${bindir}/mdio-tool"


