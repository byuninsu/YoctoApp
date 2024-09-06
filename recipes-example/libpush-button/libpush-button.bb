SUMMARY = "Push Button Application"
DESCRIPTION = "Push Button application for Yocto"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

PN = "libpush-button"

SRC_URI = "file://libpush_button.c \
           file://libpush_button.h"
           
S = "${WORKDIR}"

do_compile() {
    ${CC} ${CFLAGS} -fPIC -c libpush_button.c -o libpush_button.o
    ${CC} ${LDFLAGS} -shared -Wl,--hash-style=gnu -o libpush-button.so.1.0 libpush_button.o
    ln -sf libpush-button.so.1.0 libpush-button.so
}

do_install() {
    install -d ${D}${includedir}
    install -m 0644 libpush_button.h ${D}${includedir}

    install -d ${D}${libdir}
    install -m 0755 libpush-button.so.1.0 ${D}${libdir}
    ln -sf libpush-button.so.1.0 ${D}${libdir}/libpush-button.so
}

FILES_${PN} = "${libdir}/libpush-button.so.1.0 ${libdir}/libpush-button.so"
FILES_${PN}-dev += "${includedir}/libpush_button.h"

