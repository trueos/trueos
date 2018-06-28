TrueOS Release System:
--------------

The TrueOS release system allows a variety of build options beyond
just the typical "make release" process. By using integrated poudriere,
it is possible to provide a JSON manifest of ports / packages to build
for inclusion either on the ISO as a .txz package for install, or installed
directly into the ISO image itself. This can be used for example to build
a graphical installer by including X and related packages.


The Manifest File:
--------------

TrueOS includes an example 'trueos-manifest.json' file in this directory
which can be customized with the following JSON settings:

* ports-url - JSON String - Set to Git URL used to clone your ports tree.

```
  "ports-url":"https://github.com/trueos/trueos-ports"
```

* ports-branch - JSON String, set to the Git branch used to clone your ports tree.

```
  "ports-branch":"trueos-master"
```

* ports-conf - JSON Object Array, list per-line settings you want added to make.conf for build.
```
  "ports-conf":[
    "devel_git_SET=SVN"
  ]
```

* package-all - JSON Boolean, set to true/false if you want to do complete pkg build (AKA 'bulk -a').

```
  "package-all":false
```

* packages - JSON Object Array, list of packages to build specifically.

```
  "packages":[
    "devel/git"
  ]
```

* essential-packages - JSON Object Array, list of packages you consider critical to the build.
This is useful when doing a package-all build and don't want it to pass if something important has failed.

```
  "essential-packages":[
    "devel/git"
  ]
```

* iso-install-packages - JSON Object Array, list of packages you want pre-installed into the installation media.

```
  "iso-install-packages":[
    "devel/git"
  ]
```

* dist-packages - JSON Object Array, List of packages you want to include on install media in their pkg/txz form.

```
  "dist-packages":[
    "devel/git"
  ]
```

* auto-install-packages - JSON Object Array, List of packages you want auto-installed when using TrueOS's built-in text-installer.

```
  "auto-install-packages":[
    "devel/git"
  ]
```
* install-overlay - JSON String, directory to overlay on top of the installation media.

```
  "install-overlay":""
```

* install-script - JSON String, Command to run on boot of install media, allows custom script to be run instead of default TrueOS text installer.
```
  "install-script":"/usr/local/bin/myinstallscript"
```

* auto-install-script - JSON String, pc-sysinstall script which will be run on boot of install media. Caution, this will perform an
unattended installation when used.
```
  "install-script":"/usr/local/etc/autoinstall.cfg"
```

Poudriere Configuration (building ports):
--------------
As part of the `make poudriere` build stage, a poudriere instance is created and run to build all of the requested packages from the specified ports tree. The configuration of poudriere is automatically performed to ensure an optimal result for most build systems, but it is possible to further customize these settings as needed. Common customizations are allowing systems to override specific settings such as USE_TMPFS, or PARALLEL_JOBS that are more suitable for the individual build system.

* **NOTE:** When using Jenkins to manage the build process, it is often required to set the "PARALLEL_JOBS" value to a number a bit lower than the maximum number of CPU's on the system due to the administrative overhead of Jenkins. Typically MAX-2 is a good value for systems with many CPUs.

An example of the automatically-generated config file is included below for reference:
```
# Base Poudriere setup for build environment
ZPOOL=${ZPOOL}
FREEBSD_HOST=file://${DIST_DIR}
GIT_URL=${GH_PORTS}
BASEFS=${POUDRIERE_BASEFS}
# Change a couple poudriere defaults to integrate with an automated build
USE_TMPFS=data
ATOMIC_PACKAGE_REPOSITORY=no
PKG_REPO_FROM_HOST=yes
# Optimize the building of known "large" ports (if selected for build)
ALLOW_MAKE_JOBS_PACKAGES="chomium* iridium* gcc* webkit* llvm* clang* firefox* ruby* cmake* rust*"
PRIORITY_BOOST="pypy* openoffice* iridium* chromium*"
```

* poudriere-conf - JSON String Array, Additional configuration lines to be placed into the auto-generated poudriere.conf for the build.
```
 "poudriere-conf" : [
   "USE_TMPFS=all",
   "NOLINUX=yes",
   "PARALLEL_JOBS=30"
 ]
```

* */etc/poudriere.conf.release* - If this file exists, it will be appended as a whole to the auto-generated config.


Build Instructions:
--------------
The following instructions may be used to generate TrueOS installation
images:

```
make buildworld buildkernel
make packages
cd release && make poudriere && make release
```
