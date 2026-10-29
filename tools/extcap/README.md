# OpenThread Wireshark Extcap Sniffer (`ot-extcap`)

## Overview

`ot-extcap` is a lightweight, native C++ Wireshark extcap plugin for OpenThread. It enables seamless wireless packet sniffing of IEEE 802.15.4 / Thread networks directly within the Wireshark GUI by interfacing with an OpenThread Radio Co-Processor (RCP).

Key features include:

- **Radio URL Transport Support**: Capture from OpenThread Radio URLs (such as serial UART `spinel+hdlc+uart://` or simulation RCPs via `spinel+hdlc+forkpty://`).
- **Rich MAC Metadata**: Leverages the OpenThread stack to enrich captured frames with RSSI, LQI, and high-precision hardware/host timestamps in standard PCAPNG format (`LINKTYPE_IEEE802_15_4_TAP`).
- **Debug-Level Logging**: Provides comprehensive OpenThread (`OT_LOG_LEVEL_DEBG`) and low-level driver diagnostic logging to syslog for effortless troubleshooting.

---

## Building

`ot-extcap` is structured as a standalone root CMake project that includes OpenThread as a subproject, minimizing unnecessary dependencies and compile times.

### Using the Build Script (Recommended)

You can compile `ot-extcap` directly using the OTBR helper script:

```bash
./script/cmake-build extcap
```

The compiled executable will be located in the build directory at:

```bash
build/extcap/ot-extcap
```

### Using CMake Directly

Alternatively, configure and build using CMake and Ninja manually:

```bash
cmake -S tools/extcap -B build/extcap -G Ninja
cmake --build build/extcap
```

---

## Installation

Wireshark discovers extcap plugins by looking for executables or symbolic links located inside its system or personal extcap directory.

To install manually, symlink or copy the compiled binary into Wireshark's personal extcap directory:

1. Locate your Wireshark personal extcap directory (it depends on the Wireshark version and platform, e.g. `~/.local/lib/wireshark/extcap` with recent Wireshark versions on Linux):
   ```bash
   tshark -G folders | grep "Personal Extcap path"
   ```
2. Create a symbolic link to `ot-extcap` in that directory:

   ```bash
   EXTCAP_DIR="$(tshark -G folders | sed -n 's/^Personal Extcap path:[[:space:]]*//p')"
   mkdir -p "${EXTCAP_DIR}"
   ln -s "$(pwd)/build/extcap/ot-extcap" "${EXTCAP_DIR}/ot-extcap"
   ```

---

## Wireshark Usage

Once installed, launch Wireshark (or restart it if it was already open).

### 1. Selecting an Interface

On the Wireshark welcome screen under the **Capture** section, you will see:

- **OpenThread Sniffer over Radio URL...** (`spinel_radio_url`): Select this to connect to a custom Radio URL (such as a serial dongle or simulated RCP process).

### 2. Configuring Capture Options

Click the **gear icon** ⚙️ next to the OpenThread interface to open the configuration dialog:

- **Channel**: Select the IEEE 802.15.4 wireless channel to sniff (range: `11` to `26`, default: `11`).
- **Radio URL**: Enter the connection string (e.g., `spinel+hdlc+forkpty:///path/to/ot-rcp?forkpty-arg=1` or `spinel+hdlc+uart:///dev/ttyUSB0?uart-baudrate=460800`).
- **Verbose Debugging**: Check this box to enable full OpenThread debug-level logging (`OT_LOG_LEVEL_DEBG`) and diagnostic driver output to syslog. On Linux/macOS, these logs can be viewed via `journalctl` or system logs to diagnose hardware connection or permission errors.

### 3. Starting Capture

Click **Start** or double-click the interface name in Wireshark. `ot-extcap` will initialize the RCP in promiscuous mode, set the specified channel, and begin streaming enriched IEEE 802.15.4 frames directly into your Wireshark live capture window.

---

## Command-Line Verification

You can verify that Wireshark correctly recognizes the plugin using `tshark`:

```bash
# List all Wireshark interfaces and verify Spinel/OpenThread interfaces appear
tshark -D | grep -i spinel
```

You can also test the extcap tool directly from the terminal:

```bash
# View command-line help and usage
./build/extcap/ot-extcap --help

# List interfaces
./build/extcap/ot-extcap --extcap-interfaces

# View configuration arguments for the custom Radio URL interface
./build/extcap/ot-extcap --extcap-config --extcap-interface spinel_radio_url
```

---

## Security Considerations

### Radio URL and Arbitrary Code Execution

`ot-extcap` supports custom Radio URLs, including the `spinel+hdlc+forkpty://` scheme, which forks and executes a specified binary.

> [!WARNING]
>
> **Do NOT run `ot-extcap` with elevated privileges (such as `setuid` root).** If `ot-extcap` is run with elevated privileges, a local user could exploit the `forkpty://` transport to execute arbitrary code with those privileges.
>
> To capture from serial devices without root privileges:
>
> - On Linux, add your user to the `dialout` (or `uucp`) group: `sudo usermod -aG dialout $USER` (requires logout/login to take effect).
> - Do not attempt to run Wireshark as root or set `setuid` on the `ot-extcap` binary.
