SUMMARY = "STM32 Control Application"
DESCRIPTION = "STM32 Control application for Yocto"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

PN = "stm32-control"

SRC_URI = "file://stm32_control.c \
           file://stm32_control.h"
           
S = "${WORKDIR}"

do_compile() {
    ${CC} ${CFLAGS} -c stm32_control.c -o stm32_control.o
    ar rcs libstm32-control.a stm32_control.o
}

do_install() {
    install -d ${D}${includedir}
    install -m 0644 stm32_control.h ${D}${includedir}

    install -d ${D}${libdir}
    install -m 0644 libstm32-control.a ${D}${libdir}
}

FILES_${PN} = "${includedir}/stm32_control.h ${libdir}/libstm32-control.a"

