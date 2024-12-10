SUMMARY = "Ethernet Echo Test Application"
DESCRIPTION = "An ethernet echo application"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

SRC_URI = "file://ethernet_test.c"

S = "${WORKDIR}"

DEPENDS = "led-control \
           discrete-in \
           nvram-control \
           cjson \
           usb-control \
           stm32-control \
           rs232-control \
           ethernet-control \
           optic-control \
           bit-manager"

do_compile() {
    ${CC} ${CFLAGS} ${LDFLAGS} \
    -I${STAGING_INCDIR} \
    -I${STAGING_INCDIR}/bit-manager \
    -I${STAGING_INCDIR}/led-control \
    -I${STAGING_INCDIR}/discrete-in \
    -I${STAGING_INCDIR}/nvram-control \
    -I${STAGING_INCDIR}/optic-control \
    -I${STAGING_INCDIR}/cjson \
    -I${STAGING_INCDIR}/usb-control \
    -I${STAGING_INCDIR}/stm32-control \
    -I${STAGING_INCDIR}/rs232-control \
    -I${STAGING_INCDIR}/ethernet-control \
    -L${STAGING_LIBDIR} \
    -o ethernet-test ethernet_test.c \
    -lbit-manager \
    -lled-control \
    -ldiscrete-in \
    -lnvram-control \
    -loptic-control \
    -lcjson \
    -lusb-control \
    -lstm32-control \
    -lrs232-control \
    -lethernet-control
}

do_install() {
    install -d ${D}${bindir}
    install -m 0755 ethernet-test ${D}${bindir}
}

FILES_${PN} = "${bindir}/ethernet-test"

