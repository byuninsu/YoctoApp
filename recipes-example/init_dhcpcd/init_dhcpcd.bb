SUMMARY = "init dhcpcd script"
DESCRIPTION = "init dhcpcd script"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

PN = "init_dhcpcd"

SRC_URI = "file://init_dhcpcd"  

do_install() {
    install -d ${D}${sysconfdir}/init.d/
    install -m 0755 ${WORKDIR}/init_dhcpcd ${D}${sysconfdir}/init.d/dhcpcd  
}


RDEPENDS_${PN} += "dhcpcd"


INITSCRIPT_NAME = "dhcpcd"
INITSCRIPT_PARAMS = "defaults"
inherit update-rc.d

