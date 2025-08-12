SUMMARY = "ethernet-stp Application"
DESCRIPTION = "Ethernet STP (Spanning Tree Protocol) application for Yocto"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

DEPENDS += "ethernet-control"


SRC_URI = "file://ethernet_stp.c"

S = "${WORKDIR}"

do_compile() {
    ${CC} ${CFLAGS} -I${STAGING_INCDIR} \
    -c ethernet_stp.c -o ethernet-stp.o

    ${CC} ethernet-stp.o -o ethernet-stp \
    ${LDFLAGS} -L${STAGING_LIBDIR} -lethernet-control -lpthread
}

do_install() {
    install -d ${D}${bindir}
    install -m 0755 ethernet-stp ${D}${bindir}
}

FILES_${PN} = "${bindir}/ethernet-stp"

