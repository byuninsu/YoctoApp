SUMMARY = "Init script for bootcondition"
DESCRIPTION = "This recipe installs a custom init script to know bootcondition on boot."
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

PN = "init_bootcondition"

inherit update-rc.d

SRC_URI = "file://init_bootcondition"


do_install() {
    install -d ${D}${sysconfdir}/init.d
    install -m 0755 ${WORKDIR}/init_bootcondition ${D}${sysconfdir}/init.d/
}

INITSCRIPT_NAME = "init_bootcondition"
INITSCRIPT_PARAMS = "defaults"

