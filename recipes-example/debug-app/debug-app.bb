SUMMARY = "debug-app Application"
DESCRIPTION = "debug-app application for Yocto"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"


PN = "debug-app"
DEPENDS += "watchdog-control"
DEPENDS += "usb-control"
DEPENDS += "stm32-control"
DEPENDS += "rs232-control"
DEPENDS += "push-button"
DEPENDS += "nvram-control"
DEPENDS += "led-control"
DEPENDS += "ethernet-control"
DEPENDS += "discrete-in"
DEPENDS += "optic-control"


SRC_URI = "file://debug_app.c"
           
S = "${WORKDIR}"

do_compile() {
    ${CC} ${CFLAGS} ${LDFLAGS} \
    -I${STAGING_INCDIR}/watchdog-control \
    -I${STAGING_INCDIR}/usb-control \
    -I${STAGING_INCDIR}/stm32-control \
    -I${STAGING_INCDIR}/rs232-control \
    -I${STAGING_INCDIR}/push-button \
    -I${STAGING_INCDIR}/nvram-control \
    -I${STAGING_INCDIR}/led-control \
    -I${STAGING_INCDIR}/discrete-in \
    -I${STAGING_INCDIR}/optic-control \
    -o debug-app debug_app.c  \
    -lwatchdog-control \
    -lusb-control \
    -lstm32-control \
    -lrs232-control \
    -lpush-button \
    -lnvram-control \
    -lled-control \
    -lethernet-control \
    -ldiscrete-in \
    -loptic-control
}


do_install() {
    install -d ${D}${bindir}
    install -m 0755 debug-app ${D}${bindir}
}

FILES_${PN} = "${bindir}/debug-app"

