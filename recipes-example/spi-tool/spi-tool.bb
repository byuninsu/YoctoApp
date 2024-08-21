SUMMARY = "spi test tool Application"
DESCRIPTION = "spi test tool application for Yocto"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"
PACKAGE_NAME = "spi-tool"

PN = "spi-tool"

SRC_URI = "file://spi_tool.c"
S = "${WORKDIR}"


do_compile() {
    ${CC} ${CFLAGS} ${LDFLAGS} -o spi-tool spi_tool.c
}


do_install() {
    install -d ${D}${bindir}
    install -m 0755 spi-tool ${D}${bindir}
}

FILES_${PN} = "${bindir}/spi-tool"


