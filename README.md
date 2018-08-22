[![Build Status][otbr-travis-svg]][otbr-travis]
[![Coverage Status][otbr-codecov-svg]][otbr-codecov]

---

# OpenThread Border Router

Per the [Thread 1.1.1 Specification](http://threadgroup.org/ThreadSpec), a Thread Border Router connects a Thread network to other IP-based networks, such as Wi-Fi or Ethernet. A Thread network requires a Border Router to connect to other networks.

A Thread Border Router minimally supports the following functions:

-  End-to-end IP connectivity via routing between Thread devices and other external IP networks
-  External Thread Commissioning (for example, a mobile phone) to authenticate and join a Thread device to a Thread network

OpenThread's implementation of a Border Router is called OpenThread Border Router (OTBR).  OTBR includes a number of features, including:

-  Web UI for configuration and management
-  Thread Border Agent to support an External Commissioner
-  DHCPv6 Prefix Delegation to obtain IPv6 prefixes for a Thread network
-  NAT64 for connecting to IPv4 networks
-  DNS64 to allow Thread devices to initiate communications by name to an IPv4-only server
-  Thread interface driver using [wpantund](https://github.com/openthread/wpantund)

More information about Thread can be found at [threadgroup.org](http://threadgroup.org/). Thread is a registered trademark of the Thread Group, Inc.

[otbr-travis]: https://travis-ci.org/openthread/borderrouter
[otbr-travis-svg]: https://travis-ci.org/openthread/borderrouter.svg?branch=master
[otbr-codecov]: https://codecov.io/gh/openthread/borderrouter
[otbr-codecov-svg]: https://codecov.io/gh/openthread/borderrouter/branch/master/graph/badge.svg

## Getting started

All end-user documentation and guides are located at [openthread.io](https://openthread.io/guides/border_router). If you're looking to do things like...

- Learn about the OTBR architecture
- See what platforms support OTBR
- Build and configure OTBR

...then [openthread.io](https://openthread.io/guides/border_router) is the place for you.

If you're interested in contributing to OpenThread Border Router, read on.

# Contributing

We would love for you to contribute to OpenThread Border Router and help make it even better than it is today! See our [Contributing Guidelines](https://github.com/openthread/borderrouter/blob/master/CONTRIBUTING.md) for more information.

Contributors are required to abide by our [Code of Conduct](https://github.com/openthread/borderrouter/blob/master/CODE_OF_CONDUCT.md) and [Coding Conventions and Style Guide](https://github.com/openthread/borderrouter/blob/master/STYLE_GUIDE.md).

We follow the philosophy of [Scripts to Rule Them All](https://github.com/github/scripts-to-rule-them-all).

# Versioning

OpenThread Border Router follows the [Semantic Versioning guidelines](http://semver.org/) for release cycle transparency and to maintain backwards compatibility. OpenThread Border Router's versioning is independent of the Thread protocol specification version but will clearly indicate which version of the specification it currently supports.

# License

OpenThread Border Router is released under the [BSD 3-Clause license](https://github.com/openthread/borderrouter/blob/master/LICENSE). See the [`LICENSE`](https://github.com/openthread/borderrouter/blob/master/LICENSE) file for more information.

Please only use the OpenThread name and marks when accurately referencing this software distribution. Do not use the marks in a way that suggests you are endorsed by or otherwise affiliated with Nest, Google, or The Thread Group.

# Need help?

There are numerous avenues for OpenThread support:

* Bugs and feature requests — [submit to the Issue Tracker](https://github.com/openthread/borderrouter/issues)
* Stack Overflow — [post questions using the `openthread` tag](http://stackoverflow.com/questions/tagged/openthread)
* Google Groups — [discussion and announcements at openthread-users](https://groups.google.com/forum/#!forum/openthread-users)

The openthread-users Google Group is the recommended place for users to discuss OpenThread and interact directly with the OpenThread team.

## OpenThread

To learn more about OpenThread, see the [OpenThread repository](https://github.com/openthread/openthread).
