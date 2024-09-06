SUMMARY = "mcu-watchdog Application"
DESCRIPTION = "mcu-watchdog application for Yocto"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"
PACKAGE_NAME = "mcu-watchdog"

PN = "mcu-watchdog"

DEPENDS += "stm32-control"

SRC_URI = "file://mcu_watchdog.c \
           file://mcu_watchdog.h "

S = "${WORKDIR}"

do_compile() {

    ${CC} ${CFLAGS} ${LDFLAGS} \
    -I${STAGING_INCDIR}/stm32-control \
    -c mcu_watchdog.c -o mcu-watchdog.o
    
    ${CC} ${LDFLAGS} mcu_watchdog.o -o mcu-watchdog \
    -lstm32-control 

}

do_install() {
    install -d ${D}${bindir}
    install -m 0755 mcu-watchdog ${D}${bindir}
}

FILES_${PN} = "${bindir}/mcu-watchdog"

