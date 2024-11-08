SUMMARY = "Ethernet Echo Test Application"
DESCRIPTION = "An ethernet echo application"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

SRC_URI = "file://ethernet_test.c"

S = "${WORKDIR}"

DEPENDS = "led-control discrete-in"

do_compile() {
    ${CC} ${CFLAGS} ${LDFLAGS} \
    -I${STAGING_INCDIR} -L${STAGING_LIBDIR} \
    -o ethernet-test ethernet_test.c \
    -ldiscrete-in -lled-control
}

do_install() {
    install -d ${D}${bindir}
    install -m 0755 ethernet-test ${D}${bindir}
}

FILES_${PN} = "${bindir}/ethernet-test"

