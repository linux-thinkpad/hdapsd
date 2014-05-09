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

 * `--cfgfile` which allows to load a configuration from a file.
 * `--device` which device to protect, e.g. `--device=sda`. Defaults to
   autodetection of all rotating devices.
 * `--sensitivity` adjusts the sensitivity of the algorithmus. Defaults to 15.
 * `--adaptive` enables adaptive mode, where `hdapsd` adjusts the sensitity
   while the mouse and keyboard are used.
 * `--background` sends `hdapsd` into the background as a daemon.

For more options, please read `man hdapsd`.

systemd and udev integration
----------------------------

`hdapsd` comes with `systemd` and `udev` integration. This means when those two
are found on your system, `misc/hdapsd@.service` and `misc/hdapsd.rules` are
installed and used. `udev` will start one `hdapsd` instance for each
rotational, non-removable disk it finds.

If you want to disable this automation for a certain disk, you can mask
the `systemd` unit by calling:

    systemctl mask hdapsd@sdX

If you want to disable this automation at all, you can create an empty
`/etc/udev/rules.d/hdapsd.rules`, which will override the system-installed
udev rule.

If you want to customize the parameters `hdapsd` is using, you can edit
`/etc/hdapsd.conf` (preferred) or by customizing `hdapsd@.service` in
`/etc/systemd/system/`.

As an alternative, you could also use `misc/hdapsd.service`, which you'd
have to install yourself. This unit will just start `hdapsd` the same way
good old `sysvinit` would do.

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
Additionally the tp_smapi version provides an input interface to the data,
which stops `hdapsd` from polling the data itself all the time, saving your
battery.

Travis CI build status
----------------------
[![Build Status](https://travis-ci.org/evgeni/hdapsd.png?branch=master)](https://travis-ci.org/evgeni/hdapsd)
