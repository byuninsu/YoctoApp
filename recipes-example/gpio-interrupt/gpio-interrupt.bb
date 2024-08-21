SUMMARY = "gpio-interrupt Application"
DESCRIPTION = "gpio-interrupt application for Yocto"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

DEPENDS = "libgpiod"

SRC_URI = "file://gpio_interrupt.c"
S = "${WORKDIR}"

do_compile() {
    ${CC} ${CFLAGS} ${LDFLAGS} -o gpio-interrupt gpio_interrupt.c -lgpiod
}

do_install() {
    install -d ${D}${bindir}
    install -m 0755 gpio-interrupt ${D}${bindir}
}

FILES_${PN} = "${bindir}/gpio-interrupt"

