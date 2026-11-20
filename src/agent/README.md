# OTBR Agent

`otbr-agent` is the OpenThread Border Router daemon. It manages the OpenThread stack (in RCP or NCP mode) and connects the Thread network to external IP networks, providing Border Routing, Border Agent, Backbone Router, DNS-SD/mDNS, REST API, and D-Bus services depending on the build configuration.

## Synopsis

```text
otbr-agent [options] RADIO_URL [RADIO_URL ...]
```

At least one non-TREL Spinel `RADIO_URL` is required unless `-h`/`--help` or `-V`/`--version` is passed. Up to `OT_PLATFORM_CONFIG_MAX_RADIO_URLS` (`2`) URLs can be supplied, such as an IEEE 802.15.4 Spinel RCP URL paired with a `trel://<ifname>` URL.

Run `otbr-agent -h` to display the exact CLI options and Radio URL parameters enabled in your build.

## Command-Line Options

| Option | Argument | Default | Description |
| --- | --- | --- | --- |
| `-I`, `--thread-ifname` | `<ifname>` | `wpan0` | Name of the Thread network interface. See [Thread and Backbone Interfaces](#thread-and-backbone-interfaces). |
| `-B`, `--backbone-ifname` | `<ifname>` | _(none)_ | Name of the backbone (infrastructure) network interface (for example, `eth0`, `wlan0`, or `en0`). Can be specified multiple times. See [Thread and Backbone Interfaces](#thread-and-backbone-interfaces). |
| `-d`, `--debug-level` | `<0-7>` | `6` (`INFO`) on non-Android platforms; on Android, `5` (`NOTICE`) on `user` builds and `7` (`DEBUG`) otherwise | Log verbosity using syslog severity levels: `0` (`EMERG`), `1` (`ALERT`), `2` (`CRIT`), `3` (`ERR`), `4` (`WARNING`), `5` (`NOTICE`), `6` (`INFO`), `7` (`DEBUG`). Sets the OTBR log level, and in RCP mode also sets the OpenThread dynamic log level (capped by compile-time `OT_LOG_LEVEL`, which defaults to `INFO` unless `CMAKE_BUILD_TYPE=Debug` or `-DOT_LOG_LEVEL=DEBG`). |
| `-v`, `--verbose` | _(none)_ | Disabled | Also write logs to `stderr` (`LOG_PERROR`) when syslog is enabled. Ignored if `-s` is set, and has no effect on Android builds. |
| `-s`, `--syslog-disable` | _(none)_ | Disabled | Disable syslog (and Android logcat on Android) and write logs only to `stderr`. |
| `-h`, `--help` | _(none)_ | _(n/a)_ | Print usage text and build-specific Radio URL help to `stderr` and exit. |
| `-V`, `--version` | _(none)_ | _(n/a)_ | Print the `otbr-agent` version (`OTBR_PACKAGE_VERSION`) to `stdout` and exit. |
| `--radio-version` | _(none)_ | Disabled | Initialize the coprocessor in dry-run mode (without creating the Thread interface or starting services), print the coprocessor version string to `stdout`, and exit. Requires a `RADIO_URL`. |
| `--auto-attach` | `[=0\|1]` | `1` (enabled) | Automatically attach to a previously saved Thread network on startup (RCP mode). Because the argument is optional, attach it with `=` (no space): `--auto-attach=0` disables auto-attach; `--auto-attach` or `--auto-attach=1` (or any non-zero integer) enables it. |
| `--data-path` | `<dir>` | `/var/lib/thread` (build-time `OT_POSIX_SETTINGS_PATH`) | Directory for OpenThread persistent settings files (RCP mode). Created automatically if missing (parent directory must exist). |
| `--rest-listen-address` | `<addr\|path>` | `127.0.0.1` | Network address (IPv4 or IPv6) for the HTTP [REST API](../rest/README.md) server, or a Unix domain socket path when the value starts with `/` (for example, `/run/otbr-rest.sock`). Always parsed, but only active in RCP mode when built with `-DOTBR_REST=ON`. |
| `--rest-listen-port` | `<port>` | `8081` | TCP port for the HTTP REST API server. Ignored when `--rest-listen-address` is a Unix domain socket path. Always parsed, but only active in RCP mode when built with `-DOTBR_REST=ON`. |
| `--vendor-name` | `<name>` | _(none)_ | Vendor name for Border Agent MeshCoP mDNS TXT records and OpenThread network diagnostics (RCP mode). Only available when `OTBR_VENDOR_NAME` is not defined at build time (`script/setup` defines it by default); when available, it is required for all invocations other than `-h`/`--help` and `-V`/`--version` (including `--radio-version`). |
| `--model-name` | `<name>` | _(none)_ | Model (product) name for Border Agent MeshCoP mDNS TXT records and OpenThread network diagnostics (RCP mode). Only available when `OTBR_PRODUCT_NAME` is not defined at build time (`script/setup` defines it by default); when available, it is required for all invocations other than `-h`/`--help` and `-V`/`--version` (including `--radio-version`). |

## Thread and Backbone Interfaces

### Thread Interface (`-I` / `--thread-ifname`)

- **Linux**: `otbr-agent` automatically creates the TUN interface (default `wpan0`) through `/dev/net/tun` at startup and removes it when the daemon exits. You do not need to create `wpan0` beforehand. In RCP mode the TUN device is opened exclusively (`IFF_TUN_EXCL`), so the chosen name must not already exist. You can pick any valid Linux interface name (for example, `-I wpan1`).
- **macOS**: `otbr-agent` uses a `utun` interface. Passing `-I utunN` (such as `-I utun100`, the default for the installed launchd service) requests that specific `utun` unit. Leaving `-I` at its default (`wpan0`) lets the kernel assign the lowest available `utun` interface. Other custom interface names are rejected on macOS.
- **`ot-ctl` socket**: The Thread interface name also determines the Unix daemon socket name used by `ot-ctl`. When the active Thread interface name is not `wpan0` (including kernel-assigned `utunN` on macOS), pass `-I <ifname>` to `ot-ctl` as well (for example, `sudo ot-ctl -I wpan1 state` or `sudo ot-ctl -I utun100 state`).

### Backbone Interface (`-B` / `--backbone-ifname`)

- Specifies the host infrastructure interface (such as `eth0`, `wlan0`, or `en0`) used for Border Routing, Backbone Router, mDNS/DNS-SD proxies, and NAT64 masquerading.
- When `-B` is omitted, `otbr-agent` starts without a backbone interface, which disables features that require an infrastructure link (such as Border Routing and Backbone Router).
- `-B` can be passed multiple times (for example, `-B eth0 -B wlan0`). With multiple candidates, `otbr-agent` monitors interface link state and selects the best available link (preferring `up and running` over `up`, `down`, and `invalid`). If the active interface loses its link while another candidate is up and running, `otbr-agent` waits 10 seconds before triggering a link switch and exiting with a non-zero status so the service supervisor restarts it on the new backbone interface.

## Radio URLs

Each positional `RADIO_URL` uses the format:

```text
{Protocol}://${PATH_TO_DEVICE}?${Parameters}
```

Multiple query parameters are separated by `&`. When running `otbr-agent` from an interactive shell, quote Radio URLs containing `?` or `&` so the shell does not interpret them. Unknown or unsupported query parameters cause `otbr-agent` to exit with an invalid-arguments error.

Common schemes supported by OpenThread's POSIX platform:

| Scheme | Transport | Notes |
| --- | --- | --- |
| `spinel+hdlc+uart://<device>?<params>` | Serial / USB UART (HDLC-Lite) | Enabled by default (`OPENTHREAD_POSIX_CONFIG_SPINEL_HDLC_INTERFACE_ENABLE=1`). Default baud rate is `460800`. Example: `spinel+hdlc+uart:///dev/ttyACM0?uart-baudrate=460800`. |
| `spinel+spi://<device>?<params>` | SPI (Linux only) | Requires `-DOT_POSIX_RCP_SPI_BUS=ON` (`OPENTHREAD_POSIX_CONFIG_SPINEL_SPI_INTERFACE_ENABLE=1`). Example: `spinel+spi:///dev/spidev0.0?gpio-int-device=/dev/gpiochip0&gpio-int-line=23&gpio-reset-device=/dev/gpiochip0&gpio-reset-line=24&spi-speed=1000000`. |
| `spinel+hdlc+forkpty://<binary>?forkpty-arg=<arg>` | PTY subprocess | Forks a local RCP binary (such as simulated `ot-rcp`) for testing and simulation (`OPENTHREAD_POSIX_CONFIG_RCP_PTY_ENABLE=1`, enabled by default). `forkpty-arg` can be repeated. |
| `spinel+hdlc+unix://<socket-path>?<params>` | Unix domain stream socket | Connects to a local Unix stream socket carrying HDLC-framed Spinel (`OPENTHREAD_POSIX_CONFIG_SPINEL_HDLC_INTERFACE_ENABLE=1`). |
| `trel://<ifname>` | Thread Radio Encapsulation Link | Optional second URL enabling TREL over an IP infrastructure interface (for example, `trel://eth0` or `trel://en0`) in RCP mode; does not accept query parameters. Requires `-DOTBR_TREL=ON` (enabled by default). |

**Remote TCP RCP is not natively supported.** There is no `spinel+hdlc+tcp://` scheme. Spinel requires low-latency, deterministic communication between the host stack and the RCP, so tunneling a serial port over a network (for example, with `ser2net` and `socat`) is unsupported and prone to timeouts and RCP resets. Run `otbr-agent` directly on the host attached to the radio.

For the full list of Radio URL transports and query parameters, see the [OpenThread POSIX Radio URL documentation](https://github.com/openthread/openthread/blob/main/src/posix/README.md#radio-url) or run `otbr-agent -h`.

## Examples

### Manual Invocation on Linux

Start `otbr-agent` in the foreground with debug logging to both syslog and `stderr`:

```sh
sudo otbr-agent -I wpan0 -B eth0 -d 7 -v 'spinel+hdlc+uart:///dev/ttyACM0?uart-baudrate=460800'
```

### Multi-Radio with TREL

Use an IEEE 802.15.4 UART RCP together with TREL over Ethernet (`eth0`):

```sh
sudo otbr-agent -I wpan0 -B eth0 'spinel+hdlc+uart:///dev/ttyACM0?uart-baudrate=460800' 'trel://eth0'
```

### Checking the Coprocessor Version

```sh
sudo otbr-agent --radio-version 'spinel+hdlc+uart:///dev/ttyACM0?uart-baudrate=460800'
```

### Linux systemd Configuration (`/etc/default/otbr-agent`)

The installed `otbr-agent.service` unit reads `OTBR_AGENT_OPTS` from `/etc/default/otbr-agent` (no inner quotes around the URL are needed because systemd splits `$OTBR_AGENT_OPTS` without shell globbing):

```sh
OTBR_AGENT_OPTS="-I wpan0 -B eth0 spinel+hdlc+uart:///dev/ttyACM0?uart-baudrate=460800 trel://eth0"
```

After editing `/etc/default/otbr-agent`, restart the service:

```sh
sudo systemctl restart otbr-agent
```

### macOS launchd Configuration (`/usr/local/etc/default/otbr-agent`)

The installed `io.openthread.otbr-agent` launchd job sources `/usr/local/etc/default/otbr-agent` (with shell globbing disabled via `set -f`):

```sh
OTBR_AGENT_OPTS="-I utun100 -B en0 spinel+hdlc+uart:///dev/cu.usbserial-XXXX?uart-baudrate=460800 trel://en0"
```

After editing `/usr/local/etc/default/otbr-agent`, restart the launchd job:

```sh
sudo launchctl kickstart -k system/io.openthread.otbr-agent
```
