# XLXD (or XRFD)

The XLX Multiprotocol Gateway Reflector Server is part of the software system for the D-Star Network. The sources are published under GPL Licenses.

## Introduction

This will build **either** a new kind of XLX reflector **or** a tri-mode XRF reflector. This build support *dual-stack* operation, so the server on which it's running, must have both an IPv4 and IPv6 routable address if you are going to configure a dual-stack reflector. This XLX is different from the original because it can support out-going DExtra links, by adding a new DExtra Peer type *and* it has many changes designed to increase reliability and stability. The XRF reflector built from this source supports inbound DExtra, DPlus **and DCS connections**. Like XLX, XRF also supports out-going DExtra linking. Please note that for now, only one DExtra peer link per node is supported.

This is an improved version of the multi-protocol Reflector. Nearly all std::vector containers have been replaced with std::list containers. This is a far better choice for any collection where it is common to delete elements that are not at the end of the collection. Also in this package, no classes are derived from any standard containers. Because standard containers don't have a virtual destructor, derriving from them is highly ill-advised and while the origin XLX server worked using such derivations, it represents a possible serious problem when considering future development. Also, the clean-up routines designed to be executed when shutting down were unreachable as designed and this has been fixed. In addtion, long sleep times in certain threads were preventing a polite systemd shutdown and this has been fixed. Please note that it can still take several seconds to complete the shutdown while some execution threads complete their i/o cycle. Concurancy has been moved from std::thread to std::future. This is a superior way to manage threads. The original code created several threads that contained empty bodies and they have been eliminated. The C++ warning flag, -W has been turned on and a significant number of warning have been fixed.

The Makefiles have been improved to provide automatically generated dependencies. This significantly speeds up updating time and eliminates build errors. Be sure to do a `make clean` before you switch your build type between XLX and XRF. There are several switches in the Makefile for controlling what is to be made. By default, "make" will build an XLXD system with support for G3. The first switch will add gdb debugging support. The second switch will make a DStar-only xrfd reflector. The third switch will drop support for G3. This switch should be turned on if you are planning to have a DStar routing application also run on the server. If you want to turn on any of these switches, you should copy `Makefile` to `makefile` and then modify the `makefile`.

These reflectors no longer has a daemon-mode. It is unnecessary for systemd. Only systemd-based operating systems are supported. Debian or Ubuntu is recommended. If you want to install this on a non-systemd based OS, you are on your own. Also, by default, ambed and xlxd or xrfd are built without gdb support. If you want to add it, modify the `Makefile` in each build directory. Finally, this repository is designed so that you don't have to modify any file in the repository when you build your system. Any file you need to modify to properly configure your reflector will be a file you copy from you locally cloned repo. This makes it easier to update the source code when this repository is updated. Follow the instructions below to build your transcoding XLX reflector or tri-mode XRF reflector.

- 73 de n7tae

## Usage

The packages which are described in this document are designed to install server software which is used for the D-Star network infrastructure. It requires a 24/7 internet connection which can support 20 voice streams or more to connect repeaters and hot-spot dongles!

- The server can build a reflector that support IPv4, IPv6 or both (dual stack).
- The public IP addresses should have a DNS record which must be published in the common host files.

If you want to run this software please make sure that you can provide this service free of charge, like the developer team provides the software and they network infrastructure free of charge! If you don't need transcoding support, you don't need to build or install `ambed`.

## Installation

Below are instructions for building either an XLX or XRF reflector. It is assumed that the XLX system will support transcoding. If this is not the case, you can safely ignore commands associated with building and installing `ambed`.

### After a clean installation of Debian make sure to run update and upgrade

```bash
sudo apt update
sudo apt upgrade
```

### Required packages (some of these will probably already be installed)

```bash
sudo apt install git git-core
sudo apt install apache2 php5
sudo apt install build-essential
sudo apt install g++
```

### Download the sources

```bash
git clone https://github.com/n7tae/tae-xlxd.git
```

### Create and edit your blacklist, whitelist and linking files and systemd file

If you are building an XLX reflector:

```bash
cp ../config/xlxd.blacklist .
cp ../config/xlxd.whitelist .
cp ../config/xlxd.interlink .
cp ../config/xlxd.terminal .
cp ../systemd/xlxd.service .
```

If you are building an XRF reflector (please note the name changes, especially for the interlink file):

```bash
cp ../config/xlxd.blacklist xrfd.blacklist
cp ../config/xlxd.whitelist xrfd.whitelist
cp ../config/xlxd.interlink xrfd.interlink
cp ../config/xlxd.terminal xrfd.terminal
cp ../systemd/xrfd.service .
```

If you are not going to support G3 links, you don't need to copy the .terminal file. Use your favorite editor to modify each of these files. If you want a totally open network, the blacklist and whitelist files are ready to go. The blacklist determine which callsigns can't use the reflector. The whitelist determines which callsigns can use the reflector and the interlink or linklist file sets up the XLX<--->XLX linking and/or out-going XRF linking. When building your network, remember that XLX only supports a single hop, so each XLX reflector needs to be interlinked with all the reflectors for that module's network. Along with multi-protocol support, this is the outstanding feature of the XLX design! The down-side is that a Brand Meister link is of the same Peer group as XLX, so if you want to set up a big XLX cluster that supports transcoding, you need a transcoder for all nodes!

This limitation was the main driving force to develop this new XLX and XRF system: Transcoder hardware can be attached to an isolated XLX reflector and then linked to a larger network via XRF reflectors.

In the service file, change the `CHANGEME` to your reflector callsign, `XLX???` or `XRF???`, where `???` is a three digit number not already in use. The next two parameters are the IPv4 and IPv6 addresses to be used for binding. This should be static IP address that uses only a dotted number for IPv4 or a coloned hexidecimal number for IPv6. For either the IPv4 or the IPv6 address, you can also supply `none` and that means the module will not support that protocol. So you can configure an IPv4-only reflector, an IPv6-only reflector, or a dual stack (IPv4 *and* IPv6) reflector. Of course if you build a reflector with only IPv6, you should disable G3 support, because it's the one protocol that only operates on IPv4. For the xlxd.service file, a final parameter is the binding address for the transcoder. This can also be set to `none` and the transcoder thread will not start. If you have configured an IPv6-only reflector, you should be using an IPv6 address for the transcoder. (If it's a locally installed transcoder, use "::1".)

### Copy and edit the ambed.service file

```bash
cp ../systemd/ambed.service .
```

Make sure you are supplying the proper IP address to start ambed.

### Compile, install and start the ambed program

```bash
make -j<N>
sudo make install
```

### No for the reflector

```bash
cd ../xlxd
```

### Compile, install and start the reflector

There are several options in reflector Makefile, by default, you will not have debugging support, you will build an XLX (not an XRF) reflector and you will support Icom's G3 routing. If you want to change any of these, you need to modify you Makefile:

```bash
cp Makefile makefile
```

Edit the makefile to change your configuration. Just uncomment the lines indicated near the beginning of the file. Now your ready to comile and install:

```bash
make -j<N>
sudo make install
```

Replace the `<N>` with the number of processors on your system, which can be found with `cat /proc/cpuinfo`.

### Stoping and starting the services

```bash
sudo systemctl stop xlxd # (or xrfd)
sudo systemctl stop ambed
```

You can start each component by replacing `stop` with `start`, or you can restart each by using `restart`.

### Copy dashboard to /var/www

There are two supplied, one for XRF systems and one for XLX systems.

```bash
sudo cp -r ~/xlxd/dashboard.xlx /var/www/db     # or dashboard.xrf
```

Please note that your www root directory might be some place else. There are one file that needs configuration. Edit the copied files, not the ones from the repository:

- **pgs/config.inc.php** - At a minimum set your email address, country and comment. **Do not** enable the calling home feature if you built an XRF reflector. This feature is for **XLX systems only**.

## Updating xlxd and ambed

Go to the build directory and do a `git pull`. If you notice there is new version of the `Makefile` after you do the `git pull`, you will want to reconcile those new files with your copies **before** you make and install the executables.

## Firewall settings

XLX Server requires the following ports to be open and forwarded properly for in- and outgoing network traffic:

- TCP port 80            (http) optional TCP port 443 (https)
- TCP port 8080          (RepNet) optional
- UDP port 10001         (json interface XLX Core)
- UDP port 10002         (XLX interlink)
- TCP port 22            (ssh) optional  TCP port 10022
- UDP port 42000         (YSF protocol)
- UDP port 30001         (DExtra protocol)
- UPD port 20001         (DPlus protocol)
- UDP port 30051         (DCS protocol)
- UDP port 8880          (DMR+ DMO mode)
- UDP port 62030         (MMDVM protocol)
- UDP port 10100         (AMBE controller port)
- UDP port 10101 - 10199 (AMBE transcoding port)
- UDP port 12345 - 12346 (Icom Terminal presence and request port)
- UDP port 40000         (Icom Terminal dv port)

## YSF Master Server

Pay attention, the XLX Server acts as an YSF Master, which provides 26 wires-x rooms.
It has nothing to do with the regular YSFReflector network, hence you don’t need to register your XLX at ysfreflector.de !

## Copyright

© 2016 Jean-Luc Deltombe LX3JL and Luc Engelmann LX1IQ
© 2020 Thomas A. Early N7TAE
