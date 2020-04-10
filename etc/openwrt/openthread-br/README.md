OpenThread Border Router on OpenWRT
===================================

## Build

This is for local development.

### 1. Add OpenThread feed

Assuming `OPENWRT_TOP_SRCDIR` is root of openwrt sources.

```bash
echo src-link openthread "$(pwd)/etc/openwrt" >> ${OPENWRT_TOP_SRCDIR}/feeds.conf
cd "${OPENWRT_TOP_SRCDIR}"
./scripts/feeds update openthread
./scripts/feeds install openthread-br
```
### 2. Enable OpenThread Border Router

OpenThread is not selcted by default, so use menuconfig to select openthread-br(OpenThread Border Router).

```bash
make menuconfig
```

In the configure window, use key Up, key Down to move cursor and key Left, key Right choose action.

1. Select *Network* to enter its submenu.
2. Enable *openthread-br* by moving cursor to it and press **Y**.
3. Take *Exit* action.

### 3. Build OpenThread Border Router

```bash
make package/openthread-br/compile
```

or verbose make for debugging,

```bash
make -j1 V=sc package/openthread-br/compile
```

### 4. Install

Copy the generated **ipk** file into OpenWRT, and install with **opkg**.

```bash
opkg install openthread-br-1.0*.ipk
```

## Usage

Start otbr-agent.

```bash
# Assuming that ttyUSB0 is a RCP with baudrate 115200.
/usr/sbin/otbr-agent /dev/ttyUSB0 115200
```
