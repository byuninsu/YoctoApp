SUMMARY = "Init script and binary for GPIO event detection"
DESCRIPTION = "This recipe compiles and installs a custom C program for GPIO event detection and sets up an init script to run it on boot."
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

PN = "hold-up-callback"

DEPENDS = "libgpiod"

SRC_URI = "file://hold_up_callback \
           file://hold_up_callback.c"

do_compile() {
    ${CC} ${CFLAGS} ${LDFLAGS} -o hold_up_callback ${WORKDIR}/hold_up_callback.c -lgpiod
}

do_install() {
    install -d ${D}${sysconfdir}/init.d/
    install -m 0755 ${WORKDIR}/hold_up_callback ${D}${sysconfdir}/init.d/hold_up_callback


    install -d ${D}${bindir}
    install -m 0755 ${B}/hold_up_callback ${D}${bindir}/hold_up_callback
}

INITSCRIPT_NAME = "hold_up_callback"
INITSCRIPT_PARAMS = "defaults"

inherit update-rc.d

