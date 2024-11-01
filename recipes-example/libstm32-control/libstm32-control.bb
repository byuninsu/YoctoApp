SUMMARY = "STM32 Control Application"
DESCRIPTION = "STM32 Control application for Yocto"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

PN = "libstm32-control"

DEPENDS += "nvram-control"

SRC_URI = "file://libstm32_control.c \
           file://libstm32_control.h"
           
S = "${WORKDIR}"

do_compile() {
    ${CC} ${CFLAGS} \
    -I${STAGING_INCDIR}/nvram-control \ 
    -fPIC -c libstm32_control.c -o libstm32_control.o
    ${CC} ${LDFLAGS} -shared -Wl,--hash-style=gnu -o libstm32-control.so.1.0 libstm32_control.o
    ln -sf libstm32-control.so.1.0 libstm32-control.so
}

do_install() {
    install -d ${D}${includedir}
    install -m 0644 libstm32_control.h ${D}${includedir}/libstm32_control.h

    install -d ${D}${libdir}
    install -m 0755 libstm32-control.so.1.0 ${D}${libdir}/libstm32-control.so.1.0
    ln -sf libstm32-control.so.1.0 ${D}${libdir}/libstm32-control.so
    
}

FILES_${PN} = "${libdir}/libstm32-control.so.1.0 ${libdir}/libstm32-control.so"
FILES_${PN}-dev += "${includedir}/libstm32_control.h"


