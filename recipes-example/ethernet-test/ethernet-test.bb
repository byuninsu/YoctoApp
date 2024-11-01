SUMMARY = "ethernet Echo Test Application"
DESCRIPTION = "A ethernet echo application"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"


S = "${WORKDIR}"


SRC_URI = "file://ethernet_test.c"

do_compile() {
    ${CC} ${CFLAGS} ${LDFLAGS} -o ethernet-test ethernet_test.c
}

do_install() {
    install -d ${D}${bindir}
    install -m 0755 ethernet-test ${D}${bindir}
}

FILES_${PN} = "${bindir}/ethernet-test"

