# OpenThread Wireshark Extcap Sniffer (`ot-extcap`)

## Overview

`ot-extcap` is a lightweight, native C++ Wireshark extcap plugin for OpenThread. It enables seamless wireless packet sniffing of IEEE 802.15.4 / Thread networks directly within the Wireshark GUI by interfacing with an OpenThread Radio Co-Processor (RCP).

Key features include:

- **Universal Transport Support**: Capture from physical USB serial UART dongles (e.g., nRF52840, EFR32, CC2652) or custom Radio URLs (including simulation RCP pipes via `spinel+hdlc+fork://` and network sockets).
- **Two-Pass Automatic Discovery**: Automatically detects connected USB serial devices (`/dev/ttyACM*`, `/dev/ttyUSB*` on Linux, and `/dev/tty.usbserial-*`, `/dev/tty.usbmodem*` on macOS) across multiple baud rates (`460800` and `115200`), querying their factory-assigned IEEE EUI-64 addresses in parallel. Custom detection patterns can be specified line-by-line in `~/.config/wireshark/extcap/openthread_uart_patterns`.
- **Dynamic Reload & Config Persistence**: Dynamically scans for new USB dongles when clicking **Reload** in Wireshark and automatically saves discovered Radio URLs against their unique EUI-64 node IDs (e.g., `spinel_0x18b430000000000d` or `spinel_0x5`) across sessions in the `openthread_radio_urls` configuration file.
- **Unified Interface Names**: Uses consistent `spinel_` interface naming across physical USB dongles (`spinel_0x<eui64>`), auto-discovered UARTs (`spinel_hdlc_uart`), and custom connection URLs (`spinel_radio_url`).
- **Rich MAC Metadata**: Leverages the OpenThread stack to enrich captured frames with RSSI, LQI, and high-precision hardware/host timestamps in standard PCAPNG format.
- **Debug-Level Logging**: Provides comprehensive OpenThread (`OT_LOG_LEVEL_DEBG`) and low-level UART driver diagnostic logging to syslog for effortless hardware troubleshooting.

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

## Installation & Packaging

Wireshark discovers extcap plugins by looking for executables or symbolic links located inside its system or personal extcap directory.

### Debian / Ubuntu Package (CPack)

You can generate a standard `.deb` package using CPack:

```bash
cd build/extcap
cpack -G DEB
sudo dpkg -i ot-extcap_*.deb
```

The package includes maintainer scripts (`postinst` and `postrm`) that automatically query Wireshark (`tshark -G folders`) at install time to locate the active extcap directory and register the plugin symlink automatically.

### Manual Installation

To install manually, symlink or copy the compiled binary into Wireshark's personal extcap directory:

1. Locate your Wireshark personal extcap directory:
   ```bash
   tshark -G folders | grep "Extcap path"
   ```
2. Create a symbolic link to `ot-extcap`:

   ```bash
   # Linux (~/.config/wireshark/extcap)
   mkdir -p ~/.config/wireshark/extcap
   ln -s $(pwd)/build/extcap/ot-extcap ~/.config/wireshark/extcap/ot-extcap

   # macOS (~/Library/Application Support/Wireshark/extcap)
   mkdir -p ~/Library/Application\ Support/Wireshark/extcap
   ln -s $(pwd)/build/extcap/ot-extcap ~/Library/Application\ Support/Wireshark/extcap/ot-extcap
   ```

---

## Wireshark Usage

Once installed, launch Wireshark (or restart it if it was already open).

### 1. Selecting an Interface

On the Wireshark welcome screen under the **Capture** section, you will see:

- **OpenThread Sniffer over UART** (`spinel_hdlc_uart`): Select this to pick an auto-discovered USB serial dongle.
- **OpenThread Sniffer over Radio URL...** (`spinel_radio_url`): Select this to connect to a custom Radio URL (such as a simulated RCP process or network socket).
- **OpenThread Sniffer 0x...** (`spinel_0x...`): Any previously configured or discovered devices will be listed directly by their hexadecimal EUI-64 node ID.

### 2. Configuring Capture Options

Click the **gear icon** ⚙️ next to any OpenThread interface to open the configuration dialog:

- **Channel**: Select the IEEE 802.15.4 wireless channel to sniff (range: `11` to `26`, default: `11`).
- **Sniffer / Radio URL**:
  - For UART interfaces (`spinel_hdlc_uart`), choose an auto-discovered serial port from the **Sniffer** dropdown box (formatted as `OpenThread Sniffer 0x... (/dev/ttyACM0)`) or click **Reload** to scan for newly inserted USB dongles at `460800` and `115200` baud.
  - For custom Radio URL interfaces (`spinel_radio_url`), enter the connection string (e.g., `spinel+hdlc+fork:///path/to/ot-rcp?forkpty-arg=1` or `spinel+hdlc+uart:///dev/ttyUSB0?uart-baudrate=460800`).
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

# Scan for physical UART dongles with full OpenThread debug logging
./build/extcap/ot-extcap --detect-interfaces --debug

# View configuration arguments for the custom Radio URL interface
./build/extcap/ot-extcap --extcap-config --extcap-interface spinel_radio_url
```

---

## Security Considerations

### Radio URL and Arbitrary Code Execution

`ot-extcap` supports custom Radio URLs, including the `spinel+hdlc+fork://` scheme, which forks and executes a specified binary.

> [!WARNING]
>
> **Do NOT run `ot-extcap` with elevated privileges (such as `setuid` root).** If `ot-extcap` is run with elevated privileges, a local user could exploit the `fork://` transport to execute arbitrary code with those privileges.
>
> To capture from serial devices without root privileges:
>
> - On Linux, add your user to the `dialout` (or `uucp`) group: `sudo usermod -aG dialout $USER` (requires logout/login to take effect).
> - Do not attempt to run Wireshark as root or set `setuid` on the `ot-extcap` binary.
