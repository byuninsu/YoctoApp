SUMMARY = "Time and SSD usage logger with init script"
DESCRIPTION = "This recipe compiles and installs a custom C program for logging system time and SSD usage in JSON format and sets up an init script to run it on boot."
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

PN = "check-timestack"

DEPENDS += "nvram-control"
DEPENDS += "cjson"

SRC_URI = "file://check_timestack.c \
           file://check_timestack"

# Compilation
do_compile() {
    ${CC} ${CFLAGS} ${LDFLAGS} \
    -I${STAGING_INCDIR}/nvram-control \
    -I${STAGING_INCDIR}/cjson \
    ${WORKDIR}/check_timestack.c \
    -o ${B}/check_timestack \
    -lnvram-control \
    -lcjson 
}

# Installation
do_install() {
    install -d ${D}${sysconfdir}/init.d/
    install -m 0755 ${WORKDIR}/check_timestack ${D}${sysconfdir}/init.d/check_timestack

    install -d ${D}${bindir}
    install -m 0755 ${B}/check_timestack ${D}${bindir}/check_timestack
}

INITSCRIPT_NAME = "check_timestack"
INITSCRIPT_PARAMS = "defaults"

inherit update-rc.d

