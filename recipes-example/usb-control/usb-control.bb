SUMMARY = "usb-control Application"
DESCRIPTION = "usb-control application for Yocto"
LICENSE = "MIT"
SECTION = "libs"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

SRC_URI = "file://usb_control.c \
           file://usb_control.h"

S = "${WORKDIR}"

do_compile() {
    ${CC} ${CFLAGS} -c usb_control.c -o usb_control.o
    ar rcs libusb-control.a usb_control.o
}

do_install() {
    install -d ${D}${includedir}
    install -m 0644 usb_control.h ${D}${includedir}

    install -d ${D}${libdir}
    install -m 0644 libusb-control.a ${D}${libdir}
}


FILES_${PN}-dev += "${includedir}/usb_control.h ${libdir}/libusb-control.a"


INSANE_SKIP_${PN} = "installed-vs-shipped"

