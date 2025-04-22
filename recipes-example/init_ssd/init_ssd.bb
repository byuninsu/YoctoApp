SUMMARY = "Init script for SSD initialization"
DESCRIPTION = "This recipe installs a custom init script to initialize SSD on boot."
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

PN = "init_ssd"

SRC_URI = "file://init_ssd"

do_install() {
    install -d ${D}${sysconfdir}/init.d/
    install -m 0755 ${WORKDIR}/init_ssd ${D}${sysconfdir}/init.d/init_ssd
}

INITSCRIPT_NAME = "init_ssd"
INITSCRIPT_PARAMS = "start 18 2 3 4 5 . stop 90 0 1 6 ."

inherit update-rc.d

