SUMMARY = "nvram Control Application"
DESCRIPTION = "nvram Control application for Yocto"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"
PACKAGE_NAME = "nvram-control"

PN = "libnvram-control"

SRC_URI = "file://libnvram_control.c \
           file://libnvram_control.h"
           
S = "${WORKDIR}"

do_compile() {
    ${CC} ${CFLAGS} -fPIC -c libnvram_control.c -o libnvram_control.o
    ${CC} ${LDFLAGS} -shared -o libnvram-control.so.1.0 libnvram_control.o
    ln -sf libnvram-control.so.1.0 libnvram-control.so
}

do_install() {
    install -d ${D}${includedir}
    install -m 0644 libnvram_control.h ${D}${includedir}

    install -d ${D}${libdir}
    install -m 0755 libnvram-control.so.1.0 ${D}${libdir}
    ln -sf libnvram-control.so.1.0 ${D}${libdir}/libnvram-control.so
}

FILES_${PN} = "${libdir}/libnvram-control.so.1.0 ${libdir}/libnvram-control.so"
FILES_${PN}-dev += "${includedir}/libnvram_control.h"

