SUMMARY = "mcu-watchdog Application"
DESCRIPTION = "mcu-watchdog application for Yocto"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"


DEPENDS += "nvram-control stm32-control"

SRC_URI = "file://mcu_watchdog.c \
           file://mcu_watchdog.h"

S = "${WORKDIR}"



do_compile() {
    ${CC} ${CFLAGS} \
    -I${STAGING_INCDIR}/stm32-control \
    -I${STAGING_INCDIR}/nvram-control \
    -c mcu_watchdog.c -o mcu-watchdog.o

    ${CC} ${LDFLAGS} mcu-watchdog.o \
    -L${STAGING_LIBDIR} -lnvram-control -lstm32-control -lnvram-control \
    -o mcu-watchdog
}

do_install() {
    install -d ${D}${bindir}
    install -m 0755 mcu-watchdog ${D}${bindir}
}

FILES_${PN} = "${bindir}/mcu-watchdog"
