SUMMARY = "usb-control Dynamic Library"
DESCRIPTION = "Dynamic library for usb-control application in Yocto"
LICENSE = "MIT"
SECTION = "libs"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

SRC_URI = "file://libusb_control.c \
           file://libusb_control.h"

S = "${WORKDIR}"

do_compile() {
    ${CC} ${CFLAGS} -fPIC -c libusb_control.c -o libusb_control.o
    ${CC} ${LDFLAGS} -shared -Wl,-soname,libusb-control.so.1 -o libusb-control.so.1.0.0 libusb_control.o
    ln -sf libusb-control.so.1.0.0 libusb-control.so.1
    ln -sf libusb-control.so.1.0.0 libusb-control.so
}

do_install() {
    install -d ${D}${includedir}
    install -m 0644 libusb_control.h ${D}${includedir}

    install -d ${D}${libdir}
    install -m 0755 libusb-control.so.1.0.0 ${D}${libdir}
    ln -sf libusb-control.so.1.0.0 ${D}${libdir}/libusb-control.so.1
    ln -sf libusb-control.so.1.0.0 ${D}${libdir}/libusb-control.so
}

FILES_${PN} = "${libdir}/libusb-control.so*"
FILES_${PN}-dev += "${includedir}/libusb_control.h ${libdir}/libusb-control.so"



