# Sample Yocto recipe to build OTBR

DESCRIPTION = "OpenThread Border Router Agent"
HOMEPAGE = "https://github.com/openthread/ot-br-posix"

LICENSE = "BSD-3-Clause"
LIC_FILES_CHKSUM = "file://LICENSE;md5=87109e44b2fda96a8991f27684a7349c"

SRC_URI = "gitsm://git@github.com/openthread/ot-br-posix.git;branch=main;protocol=ssh"

ICECC_DISABLED = "1"

S = "${WORKDIR}/git"
SRCREV = "${AUTOREV}"
PV_append = "+${SRCPV}"

DEPENDS += "avahi boost dbus iproute2 jsoncpp ncurses"

inherit autotools cmake

EXTRA_OECMAKE += " \
    -GNinja \
    -Werror=n \
    -DBUILD_TESTING=OFF \
    -DCMAKE_BUILD_TYPE="Release" \
    -DOTBR_BACKBONE_ROUTER=OFF \
    -DOTBR_BORDER_ROUTING=ON \
    -DOTBR_COVERAGE=OFF \
    -DOTBR_MDNS=avahi \
    -DOTBR_SRP_ADVERTISING_PROXY=ON \
    -DOTBR_VENDOR_NAME="OpenThread" \
    -DOTBR_PRODUCT_NAME="Border Router" \
    -DOTBR_MESHCOP_SERVICE_INSTANCE_NAME="OpenThread Border Router" \
    -DOT_DUA=OFF \
    -DOT_POSIX_SETTINGS_PATH='"/tmp/"' \
    -DOT_MLR=OFF \
"
