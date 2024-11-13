SUMMARY = "Multiple Spanning Tree Protocol Daemon"
DESCRIPTION = "mstpd is an open-source daemon that implements the Spanning Tree Protocol (STP, RSTP, MSTP)"
LICENSE = "GPL-2.0-only"
LIC_FILES_CHKSUM = "file://LICENSE;md5=4325afd396febcb659c36b49533135d4"

SRC_URI = "git://github.com/mstpd/mstpd.git;branch=master;protocol=https \
           file://mstpd.conf"

PV = "0.0.8+git${SRCPV}"
SRCREV = "d27d7e93485d881d8ff3a7f85309b545edbe1fc6"

S = "${WORKDIR}/git"

inherit autotools

do_install() {
    # Install the mstpd binary
    install -d ${D}${bindir}
    install -m 0755 mstpd ${D}${bindir}/mstpd

    # Install configuration files
    install -d ${D}${sysconfdir}/mstpd
    install -m 0644 ${WORKDIR}/mstpd.conf ${D}${sysconfdir}/mstpd/mstpd.conf
}

