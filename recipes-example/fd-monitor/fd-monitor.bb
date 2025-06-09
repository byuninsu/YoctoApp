SUMMARY = "fd_monitor Application"
DESCRIPTION = "fd_monitor application for Yocto"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

SRC_URI = "file://fd_monitor.c"

S = "${WORKDIR}"

do_compile() {
    ${CC} ${CFLAGS} ${LDFLAGS} fd_monitor.c -o fd_monitor
}

do_install() {
    install -d ${D}${bindir}
    install -m 0755 fd_monitor ${D}${bindir}/fd_monitor
}

FILES:${PN} = "${bindir}/fd_monitor"

