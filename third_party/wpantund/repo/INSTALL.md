`wpantund` Installation Guide
=============================

This document describes the process of building and installing
`wpantund` on both Ubuntu and OS X. Installation on other platforms
may be possible, but are left as an excercise for the reader. This
document assumes that you are at least casually familiar with
[Autoconf][1]. It also assumes that you have already gotten a copy of
the wpantund sources, extracted them, and are wondering what to do
next.

[1]: http://www.gnu.org/software/autoconf/autoconf.html



Installing `wpantund` on Ubuntu
-------------------------------

### 1. Install Dependencies ###

Open up a terminal and perform the following commands:

	sudo apt-get update

	# Install runtine-dependent packages (libreadline is optional)
	sudo apt-get install dbus libreadline

	# Install build-dependent packages (libreadline-dev is optional)
	sudo apt-get install gcc g++ libdbus-1-dev libboost-dev libreadline-dev

### 2. Configure and build the project ###

If the `configure` script is not already present in the root directory
of your `wpantund` sources (which it should be if you got these
sources from a tarball), you will need to either grab one of the `full/*`
tags from the official git repository or run the bootstrap script.

#### 2.1. Grabbing a full tag from Git ####

The most likely thing you want to build is the latest stable release.
In that case, all you need to do is checkout the tag `full/latest-release`:

    git checkout full/latest-release

And you should then be ready to build configure. Jump to section 2.3.

#### 2.2. Running the bootstrap script  ####

Alternatively, you can *bootstrap* the project directly by doing the
following:

    sudo apt-get install libtool autoconf autoconf-archive
    ./bootstrap.sh

#### 2.3. Running the configure script  ####

If the `configure` script is present, run it and them start the make
process:

    ./configure --sysconfdir=/etc
    make

This may take a while. You can speed up the process by adding the
argument `-j4` to the call to `make`, substituting the number `4` with
the number of processor cores you have on your machine. This greatly
improves the speed of builds.

Also, if additional debugging information is required or helpful from
`wpantund`, add the argument `--enable-debug` to the `./configure`
line above.

### 3. Install `wpantund` ###

Once the build above is complete, execute the following command:

    sudo make install

This will install `wpantund` onto your computer.

Installing `wpantund` on OS X
-----------------------------

Installing `wpantund` on OS X is largely similar to the process above,
except things are complicated by the fact that we depend on D-Bus—and
there is no native package manager for OS X. These instructions assume
that you are using [Homebrew][2] as your package manager.

[2]: http://brew.sh/

What is nice about homebrew is that we have a recipe to build
`wpantund` for you. This makes installing `wpantund` on OS X as easy
as:

    brew update
    brew install ./etc/wpantund.rb

    # Start the D-Bus daemon
    sudo cp "$(brew --prefix)"/Cellar/dbus/*/org.freedesktop.dbus-session.plist /Library/LaunchDaemons/
    sudo launchctl load -w /Library/LaunchDaemons/org.freedesktop.dbus-session.plist

(the last two commands are for setting D-Bus up to launch properly at
startup)

PRO-TIP: Use `brew install wpantund --HEAD` if you want the latest
bleeding-edge version of wpantund!

However, **if you want to build `wpantund` manually**, the procedure
described below allows you to manually set up the `wpantund` dependencies
so that the build can run without a hitch.

### 1. Install Xcode ###

Go [here](http://itunes.apple.com/us/app/xcode/id497799835?ls=1&mt=12)
and install [Xcode](https://developer.apple.com/xcode/) if you haven't
already.

After you have installed Xcode, you will need to install the Xcode
command-line tools. You can do this easily from the command line with
the following command:

    xcode-select --install

### 2. Install Homebrew ###

[Homebrew](http://brew.sh/) is a package management system for OS X. While
it is possible to install wpantund's dependencies manually, using
homebrew makes this process much easier. If you don't already have it
installed on your Mac, you can install it to your home directory using
the following instructions:

    cd ~
    mkdir homebrew && curl -L https://github.com/mxcl/homebrew/tarball/master | tar xz --strip 1 -C homebrew
    mkdir ~/bin

Create the file `~/.bash_profile` with the following contents (tweak to
your preference if you know what you're doing):

    # Global stuff
    export PATH=$HOME/bin:$PATH
    export EDITOR=vi

    # Homebrew stuff
    export PATH=$HOME/homebrew/bin:$PATH

Then close and reopen your terminal window.

Alternatively, you can follow the instructions at <http://brew.sh/>, which
installs to the prefix `/usr/local` instead of `~/homebrew`.

### 2. Install and Setup `wpantund` dependencies ###

We need a few dependencies in order to be able to build and use
wpantund. The following commands will get us up and running:

	brew install pkg-config
	brew install autoconf-archive
	brew install libtool
	brew install boost
	brew install d-bus

	# Start the D-Bus daemon
	sudo cp "$(brew --prefix)"/Cellar/dbus/*/org.freedesktop.dbus-session.plist /Library/LaunchDaemons/
	sudo launchctl load -w /Library/LaunchDaemons/org.freedesktop.dbus-session.plist

### 3. Configure and build ###

At this point, you can jump over to step 2 from the section
*Installing `wpantund` on Ubuntu*, above.




Configuring and Using `wpantund`
-------------------------------

### 1. Configuring `wpantund` ###

Now that you have `wpantund` installed, you will need to edit the
configuration file to tell the daemon how to communicate with the NCP.
You do this by editing the `wpantund.conf` file, which (if you
followed the directions above) should now be at `/etc/wpantund.conf`.

This file is, by default, filled only with comments—which describe
all of the important configuration options that you might need to set
in order to make wpantund usable. Read them over and then uncomment
and update the appropriate configuration properties.

Alternatively, you can specify any needed properties on the command
line when invoking `wpantund`. At a minimum, at least `NCPSocketName`
needs to be specified, which describes how `wpantund` is supposed to
talk to the NCP.

Refer to the authorative documentation in `/etc/wpantund.conf` or
`./src/wpantund/wpantund.conf` for more information.

### 2. Start wpantund ###

To connect to an NCP on the serial port `/dev/ttyUSB0`, type the
following into terminal:

    sudo /usr/local/sbin/wpantund -o NCPSocketName /dev/ttyUSB0

To start wpan on more than one interface, you can specify the WPAN
interface name. For example, to set `wpan0` network interface on
`/dev/ttyUSB0` USB interface, and `wpan1` on `/dev/ttyUSB1`, run:

    sudo /usr/local/sbin/wpantund -o NCPSocketName /dev/ttyUSB0 -o WPANInterfaceName wpan0

and

    sudo /usr/local/sbin/wpantund -o NCPSocketName /dev/ttyUSB1 -o WPANInterfaceName wpan1

Note that, unless you are running as root, you *must* use `sudo` when
invoking `wpantund` directly.

On an embedded device, you would add the appropriate scripts or
configuration files that would cause `wpantund` to be started at boot.
Doing so should be pretty straightforward.

### 3. Using `wpanctl` ###

Now that you have `wpantund` running, you can now issue commands to
the daemon using `wpanctl` from another window: (Again, unless you are
running as root, you *must* use `sudo`)

    $ sudo /usr/local/bin/wpanctl
    wpanctl:wpan0> leave
    Leaving current WPAN. . .
    wpanctl:wpan0> setprop NetworkKey --data 000102030405060708090a0b0c0d0e0f
    wpanctl:wpan0> form "MyCoolNetwork" -c 26
    Forming WPAN "MyCoolNetwork" as node type 2
    Successfully formed!
    wpanctl:wpan0> permit-join 3600 22
    Permitting Joining on the current WPAN for 3600 seconds, commissioning traffic on TCP/UDP port 22. . .
    wpanctl:wpan0> status
    wpan0 => [
        "AssociationState" => "joined"
        "NetworkName" => "MyCoolNetwork"
        "XPanId" => 0xD6D8A04025AB3B0C
        "PanId" => 0xE3C3
        "Channel" => 26
        "AllowingJoin" => true
        "Prefix" => [FDD6D8A040250000]
        "NCPVersion" => "OpenThread/1.0d26-25-gb684c7f; DEBUG; May 9 2016 18:22:04"
        "HWAddr" => [18B430000003F202]
    ]
    wpanctl:wpan0>
