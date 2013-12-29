`hdapsd` - Hard Drive Active Protection System Daemon
===================================================

This is a disk protection user-space daemon. It monitors the acceleration
values through the various motion interfaces and automatically initiates
disk head parking if a fall or sliding of the laptop is detected.

Currently, the following motion interfaces are supported:
 * HDAPS on IBM/Lenovo ThinkPads
 * AMS on Apple iBooks and PowerBooks (G4)
 * FREEFALL on Hewlett-Packard and DELL laptops
 * HP3D on Hewlett-Packard laptops
 * APPLESMC on Apple MacBooks and MacBooks Pro (Intel) (UNTESTED!)

Compilation
-----------

    ./configure
    make
    make install

### Configure parameters

The following parameters to `./configure` are probably interesting:

 * `--with-systemdsystemunitdir` lets you specify the directory for
   systemd unit files. It defaults to the output of
   `pkg-config --variable=systemdsystemunitdir systemd`.
 * `--with-udevdir` lets you specify the directory for udev rules files.
   It defaults to the output of `pkg-config --variable=udevdir udev`.

Packages
--------
 * [Arch](https://www.archlinux.org/packages/hdapsd) and [AUR](https://aur.archlinux.org/packages/hdapsd-git/)
 * [Debian](http://packages.debian.org/hdapsd)
 * [Fedora](https://apps.fedoraproject.org/packages/hdapsd)
 * [Gentoo](https://packages.gentoo.org/package/app-laptop/hdapsd)
 * [Ubuntu](http://packages.ubuntu.com/hdapsd)

Usage
-----

In most cases, just running `hdapsd` as root should be enough, as it will
try to autodetect everything itself.

If you want to adjust stuff, these are the most commonly used options:

 * `--device` which device to protect, e.g. `--device=sda`. Defaults to
   autodetection of all rotating devices.
 * `--sensitivity` adjusts the sensitivity of the algorithmus. Defaults to 15.
 * `--adaptive` enables adaptive mode, where `hdapsd` adjusts the sensitity
   while the mouse and keyboard are used.
 * `--background` sends `hdapsd` into the background as a daemon.

For more options, please read `man hdapsd`.

Compatibility
-------------
Since kernel 2.6.28 you don't need to patch your kernel, as support for
IDLE_IMMEDIATE is present in mainline.

**NOTE**: The new interface only allows IDLE_IMMEDIATE for drives that
announce to be ATA-7 conform. But threre are also drives that support ATA-6
only but do IDLE_IMMEDIATE fine. For those you need to force the interface
with: `echo -1 > /sys/block/$DISK/device/unload_heads`.
Or you can call `hdapsd` like this: `hdapsd -f -d $DISK`, to achieve the same
result.

For kernels <2.6.28, please have a look at
http://www.thinkwiki.org/wiki/HDAPS#Kernel_patch
and patch your kernel with the appropriate patch before using `hdapsd`.

mainline hdaps module vs tp_smapi (ThinkPad only)
-------------------------------------------------
The mainline hdaps module present in Linux kernels does not support all
hdaps-enabled ThinkPads, thus it is recommended to use the one provided
by tp_smapi.
Additionally the tp_smapi version provides a input interface to the data,
which stops `hdapsd` from polling the data itself all the time, saving your
battery.

Travis CI build status
----------------------
[![Build Status](https://travis-ci.org/evgeni/hdapsd.png?branch=master)](https://travis-ci.org/evgeni/hdapsd)
