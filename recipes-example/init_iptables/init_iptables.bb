SUMMARY = "Init iptables for docker port forwarding"
DESCRIPTION = "This recipe installs a custom daemon.json for docker with iptables set to false."
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

PN = "init_iptables"

SRC_URI = "file://daemon.json"

do_install() {
    install -d ${D}${sysconfdir}/docker/
    install -m 0644 ${WORKDIR}/daemon.json ${D}${sysconfdir}/docker/daemon.json
}

INITSCRIPT_NAME = "init_iptables"
INITSCRIPT_PARAMS = "defaults"

