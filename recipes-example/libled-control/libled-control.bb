SUMMARY = "LED Control Application"
DESCRIPTION = "LED Control application for Yocto"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"
PACKAGE_NAME = "libled-control"

PN = "libled-control"

DEPENDS += "stm32-control"

SRC_URI = "file://libled_control.c \
           file://libled_control.h"
           
S = "${WORKDIR}"

do_compile() {
    ${CC} ${CFLAGS} \
        -I${STAGING_INCDIR}/stm32-control \
        -fPIC -c libled_control.c -o libled_control.o

    ${CC} ${LDFLAGS} \
        -L${STAGING_LIBDIR} -lstm32-control \
        -shared -Wl,--hash-style=gnu -o libled-control.so.1.0 libled_control.o

    ln -sf libled-control.so.1.0 libled-control.so
}

do_install() {
    install -d ${D}${includedir}
    install -m 0644 libled_control.h ${D}${includedir}

    install -d ${D}${libdir}
    install -m 0755 libled-control.so.1.0 ${D}${libdir}
    ln -sf libled-control.so.1.0 ${D}${libdir}/libled-control.so
}

FILES_${PN} = "${libdir}/libled-control.so.1.0 ${libdir}/libled-control.so"
FILES_${PN}-dev += "${includedir}/libled_control.h"
