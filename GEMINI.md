# OpenThread Border Router (OTBR) for POSIX

## Project Overview

OpenThread Border Router (OTBR) is an open-source implementation of a Thread Border Router, which connects a Thread network to other IP-based networks (e.g., Wi-Fi or Ethernet). It provides end-to-end IP connectivity, DNS-based service discovery (mDNS/SRP), DHCPv6 Prefix Delegation, NAT64, and External Thread Commissioning.

The project is primarily written in C++11 and C99, and is designed to run on POSIX-compliant operating systems like Linux. It leverages the OpenThread stack and provides additional services to act as a border router.

### Key Components

- **`otbr-agent`**: The main agent process that manages the Thread interface and coordinates other services.
- **mDNS**: Supports multiple mDNS providers including Avahi, mDNSResponder, and OpenThread's built-in mDNS.
- **Web GUI**: An optional web interface for managing the border router.
- **REST API**: A RESTful interface for monitoring and configuration.
- **DBus Server**: Provides a DBus interface for external applications to interact with OTBR.

## Building and Running

The project uses CMake as its build system and Ninja as the preferred build tool.

### Prerequisites

You can install all necessary dependencies using the bootstrap script:

```bash
./script/bootstrap
```

### Building

To build the project with default options:

```bash
./script/cmake-build
```

You can pass CMake options to the script:

```bash
./script/cmake-build -DOTBR_DBUS=ON -DOTBR_WEB=ON
```

### Running Tests

OTBR uses a comprehensive test suite including unit tests and simulation-based integration tests.

```bash
# Run all tests
./script/test build check simulation
```

## Development Conventions

### Coding Style

OTBR follows the OpenThread coding conventions, which are enforced using `clang-format`.

- **Indentation**: 4 spaces.
- **Naming**:
  - `UpperCamelCase` for classes, structures, enums, methods, and functions.
  - `lowerCamelCase` for variables and parameters.
- **Variable Prefixes**:
  - `m` for class/structure members (e.g., `mInterfaceName`).
  - `a` for function/method parameters (e.g., `aPacket`).
  - `g` for global variables (e.g., `gLogFacility`).
  - `s` for static variables (e.g., `sInstance`).
  - `k` for constant variables (e.g., `kDefaultInterfaceName`).
- **Headers**: Use Doxygen for documentation and standard include guards.

### Formatting

Always run the following script before submitting changes to ensure compliance with the style guide:

```bash
./script/make-pretty
```

To check if the code is correctly formatted:

```bash
./script/make-pretty check
```

### Documentation

Public APIs and significant internal logic should be documented using Doxygen-style comments. You can build the documentation with:

```bash
./script/test doxygen
```
