SUMMARY = "init dhcpcd and configure 1000Base-X ports"
DESCRIPTION = "Init script to start dhcpcd and configure 1000Base-X ports"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

PN = "init_dhcpcd"

SRC_URI = "file://init_dhcpcd \
           file://port2.sh \
           file://port3.sh \
           file://port4.sh \
           file://port5.sh \
           file://port6.sh \
           file://port9.sh \
           file://port10.sh \
           file://port2_default.sh \
           file://port3_default.sh \
           file://port4_default.sh \
           file://port5_default.sh \
           file://port6_default.sh \
           file://port9_default.sh \
           file://port10_default.sh"

do_install() {
    install -d ${D}${sysconfdir}/init.d/
    install -m 0755 ${WORKDIR}/init_dhcpcd ${D}${sysconfdir}/init.d/dhcpcd


    install -d ${D}${bindir}/
    install -m 0755 ${WORKDIR}/port2.sh ${D}${bindir}/port2.sh
    install -m 0755 ${WORKDIR}/port3.sh ${D}${bindir}/port3.sh
    install -m 0755 ${WORKDIR}/port4.sh ${D}${bindir}/port4.sh
    install -m 0755 ${WORKDIR}/port5.sh ${D}${bindir}/port5.sh
    install -m 0755 ${WORKDIR}/port6.sh ${D}${bindir}/port6.sh
    install -m 0755 ${WORKDIR}/port9.sh ${D}${bindir}/port9.sh
    install -m 0755 ${WORKDIR}/port10.sh ${D}${bindir}/port10.sh
    install -m 0755 ${WORKDIR}/port2.sh ${D}${bindir}/port2_default.sh
    install -m 0755 ${WORKDIR}/port3.sh ${D}${bindir}/port3_default.sh
    install -m 0755 ${WORKDIR}/port4.sh ${D}${bindir}/port4_default.sh
    install -m 0755 ${WORKDIR}/port5.sh ${D}${bindir}/port5_default.sh
    install -m 0755 ${WORKDIR}/port6.sh ${D}${bindir}/port6_default.sh
    install -m 0755 ${WORKDIR}/port9.sh ${D}${bindir}/port9_default.sh
    install -m 0755 ${WORKDIR}/port10.sh ${D}${bindir}/port10_default.sh
}

RDEPENDS_${PN} += "dhcpcd"

INITSCRIPT_NAME = "dhcpcd"
INITSCRIPT_PARAMS = "defaults"

inherit update-rc.d

