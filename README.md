[![Build Status](https://builds.ixsystems.com/jenkins/job/TrueOS%20-%20World%20CI/job/trueos-master/badge/icon)](https://builds.ixsystems.com/jenkins/job/TrueOS%20-%20World%20CI/job/trueos-master/)

TrueOS Source:
--------------

This is the top level TrueOS source directory. It is more or less a fork
of [FreeBSD](https://github.com/freebsd/freebsd).

TrueOS Differences:
--------------

In what ways does TrueOS differ from stock FreeBSD you may be wondering?
Read on for a list of the distinctions in no particular order:

* Bi-Annual Release Cycle - TrueOS follows traditional FreeBSD HEAD and cuts new releases
on a 6 months schedule which includes OS and Ports.

* GitHub - TrueOS uses Git/GitHub as the "Source of truth", Pull-Requests welcome!

* CI - Our [Jenkins build cluster](https://builds.ixsystems.com/jenkins/job/TrueOS%20-%20World%20CI/) is constantly building [new versions](https://builds.ixsystems.com/jenkins/job/TrueOS%20-%20World%20CI/job/trueos-master/) of TrueOS for testing.

* [dhcpcd](https://github.com/rsmarples/dhcpcd) - dhcpcd has been integrated directly into base,
allowing more advanced IPv4 and IPv6 network configuration

* [OpenRC](https://github.com/openrc/openrc/) - This replaces the legacy rc.d scripts with
OpenRC's init.d subsystem, allowing faster boots, as well as a host of other service improvements.

* Package Base - TrueOS is installed and based on using Packages for the Base OS.

* [pkg in base](https://github.com/freebsd/pkg) - To go along with using base system packages,
TrueOS has also integrated PKG directly in the base system.

* Root NSS Certs - Since it really is a bummer to not be able to use HTTPS out of box...

* Custom Installer - TrueOS includes its own [pc-sysinstall](https://github.com/trueos/trueos/tree/trueos-master/usr.sbin/pc-sysinstall) installation system, along with
[text-based](https://github.com/trueos/trueos/tree/trueos-master/usr.sbin/pc-installdialog) front-end. This allows a wide variety of ZFS-based installation options, as well
as scriptability.

* More as they come...

Build Instructions:
--------------
The following instructions may be used to generate TrueOS installation
images:

```
make buildworld buildkernel packages
cd release && make release
```
