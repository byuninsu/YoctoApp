SUMMARY = "RS232 Echo Test Application"
DESCRIPTION = "A UART echo application for RS232 communication testing on Jetson"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"


S = "${WORKDIR}"


SRC_URI = "file://rs232_test.c"

do_compile() {
    ${CC} ${CFLAGS} ${LDFLAGS} -o rs232-test rs232_test.c
}

do_install() {
    install -d ${D}${bindir}
    install -m 0755 rs232-test ${D}${bindir}
}

FILES_${PN} = "${bindir}/rs232-test"

