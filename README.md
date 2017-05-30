# OpenThread Border Router

Per the [Thread 1.1.1 Specification](http://threadgroup.org/ThreadSpec), a Border Router connects a 802.15.4 network to networks at different layers, such as WiFi or Ethernet.  A Thread network requires a Border Router to connect to other networks.

A Thread Border Router minimally supports the following functions:

-  End-to-end IP connectivity via routing between Thread devices and other external IP networks
-  External Thread Commissioning (for example, a mobile phone) to authenticate and join a Thread device to a Thread network

OpenThread's implementation of a Border Router is called OpenThread Border Router (OTBR).  OTBR includes a number of features, including:

-  Web UI for configuration and management
-  Thread Border Agent to support an External Commissioner
-  NAT64 for connecting to IPv4 networks
-  Thread interface driver using wpantund

> **Note:** This is an early MVP release of OpenThread Border Router that will be improved over time. See the [Roadmap](https://github.com/openthread/borderrouter/wiki/Roadmap) page to view major fixes in progress, or the [Issues](https://github.com/openthread/borderrouter/issues) page to report bugs and request enhancements.<br /><br />Pull requests are encouraged and welcome. See the [`CONTRIBUTING.md`](https://github.com/openthread/borderrouter/blob/master/CONTRIBUTING.md) file for more information.

## Border Agent

The Border Agent binds to both Thread and WAN (WiFi, Ethernet) interfaces, to support an External Thread Commissioner in authenticating and joining Thread devices."

![OTBR Architecture](https://gist.githubusercontent.com/Vyrastas/0489e2c081c0444c2462dcee54962cb7/raw/100a8a08f1e9103672017f0f05eac6f85cb8e31a/otbr-arch-borderagent_2x.png)

It also provides support for the NCP design, where Thread functions are offloaded to an NCP and OTBR runs on the host side.  In this design, the Border Agent communicates with the NCP via [`wpantund`](https://github.com/openthread/openthread/wiki/Wpantund-Docs) and [Spinel](https://github.com/openthread/openthread/blob/master/doc/draft-spinel-protocol.txt).  A Border Agent Proxy provides an interface between Spinel and OpenThread's CoAP client and server components on the NCP.  For communication with an external Commissioner, the Border Agent uses standard UDP sockets.

![OTBR NCP Architecture](https://gist.githubusercontent.com/Vyrastas/0489e2c081c0444c2462dcee54962cb7/raw/8ca4195936c867adaf7fb74058fef7c622007361/otbr-arch-borderagent-ncp_2X.png)

The Border Agent utilizes the following third-party components:

-  libcoap — An open source C-implementation of CoAP implementation
-  mbed TLS — Supports DTLS communication with an external Commissioner

## Border Router Services

OTBR also provides the following services:

-  mDNS Publisher — allows an External Commissioner to discover an OTBR and its associated Thread network
-  PSKc Generator — for generation of PSKc keys
-  Web Service — web UI for management of a Thread network
-  WPAN Controller — DBus operations for control of the WPAN interface

Third-party components for Border Router Services include Simple Web Server and Material Design Lite for the framework of the web UI.

# Need help?

## Interact

There are numerous avenues for OTBR support:

-  Bugs and feature requests — [submit to the Issue Tracker](https://github.com/openthread/borderrouter/issues)
-  Stack Overflow — [post questions using the `openthread` tag](http://stackoverflow.com/questions/tagged/openthread)
-  Google Groups — discussion and announcements
   -  [openthread-announce](https://groups.google.com/forum/#!forum/openthread-announce) — release notes and new updates on OpenThread
   -  [openthread-users](https://groups.google.com/forum/#!forum/openthread-users) — the best place for users to discuss OpenThread and interact with the OpenThread team

## OpenThread

To learn more about OpenThread, see the [OpenThread repository](https://github.com/openthread/openthread).

# Want to contribute?

We would love for you to contribute to OpenThread Border Router and help make it even better than it is today! See the [`CONTRIBUTING.md`](https://github.com/openthread/borderrouter/blob/master/CONTRIBUTING.md) file for more information.

# Versioning

OpenThread Border Router follows the [Semantic Versioning guidelines](http://semver.org/) for release cycle transparency and to maintain backwards compatibility. OpenThread's versioning is independent of the Thread protocol specification version but will clearly indicate which version of the specification it currently supports.

# License

OpenThread Border Router is released under the [BSD 3-Clause license](https://github.com/openthread/borderrouter/blob/master/LICENSE). See the [`LICENSE`](https://github.com/openthread/borderrouter/blob/master/LICENSE) file for more information.

Please only use the OpenThread name and marks when accurately referencing this software distribution. Do not use the marks in a way that suggests you are endorsed by or otherwise affiliated with Nest, Google, or The Thread Group.
