TrueOS Release System:
--------------

The TrueOS release system allows a variety of build options beyond
just the typical "make release" process. By using integrated poudriere,
it is possible to provide a JSON manifest of ports / packages to build
for inclusion either on the ISO as a .txz package for install, or installed
directly into the ISO image itself. This can be used for example to build
a graphical installer by including X and related packages.

Build Instructions:
--------------
The following instructions may be used to generate TrueOS installation images:
The build manifest is supplied by setting the "TRUEOS_MANIFEST" environment variable with the path to the file.

```
make buildworld buildkernel
make packages
cd release && make release
```

The Manifest File:
--------------

TrueOS includes an example '[trueos-manifest.json](trueos-manifest.json)' file in this directory
which can be customized with the following JSON settings:

### version
The JSON manifest has a version control string which must be set. 

This document corresponds to version "1.0", so any manifest using this specification needs to have the following field in the top-level object of the manifest:

`"version" : "1.0"`

### Distribution Branding
There are a couple options which may be set in the manifest in order to "brand" the distribution of TrueOS.

* "os_name" (string) : Branding name for the distribution.
   * Default Value: "TrueOS"
   * Will change the branding in pc-installdialog, and the distro branding in the bootloader as well.
* "os_version" (string) : Custom version tag for the build. 
   * At build time this will become the "TRUEOS_VERSION" environment variable which can be used in filename expansions and such later (if that environment variable is not already set).


### base-packages
The "base-packages" target allows the configuration of the OS packages itself. This can involve the naming scheme, build flags, extra dependencies, and more.

#### Base Packages Options
* "name-prefix" (string) : Naming convention for the base packages (Example: FreeBSD-runtime will become [name-prefix]-runtime)
* "depends" (JSON object) : This is a object containing declarations of additional dependencies that you would like to add to particular base packages: The next level of the object is the name of the base package, then within that object is the name of the package you want to add as a dependency, and within that is the origin and version of the package that is needed. See the example below for a working demonstration of this dependency injection.
   * **WARNING:** Make sure that only simple ports/packages are injected with this mechanism! Example: The runtime package installs the user/groups files on the system, so adding a dependency on a package that needs to create a user/group will cause install failures since the dependency is installed before the runtime package.
* "kernel-flags" and "world-flags" (JSON object) : These are objects containing extra builds flags that will be used for the kernel/world build stages. 
   * "default" (JSON array of strings) : Default list of build flags (required)
   * "ENV_VARIABLE" (JSON array of strings) : Additional list to be added to the "default" list **if** an environment variable with the same name exists.
* "strip-plist" (JSON array of strings) :  List of directories or files that need to be removed from the base-packages.

#### Base Packages Example
```
"base-packages" : {
  "name-prefix" : "TrueOS",
  "depends" : {
    "runtime": {
      "uclcmd": {
        "origin": "devel/uclcmd",
        "version": ">0"
      }
    }
  },
  "kernel-flags": {
    "default": [
      "WITH_FOO=1",
      "WITH_BAR=2"
    ],
    "ENV_VARIABLE_FOOBAR": [
      "WITH_FOOBAR=1",
      "WITHOUT_FOOBAR2=1"
    ]
  },
  "world-flags": {
    "default": [
      "WITH_FOO=1",
      "WITH_BAR=2"
    ],
    "ENV_VARIABLE_FOOBAR": [
      "WITH_FOOBAR=1",
      "WITHOUT_FOOBAR2=1"
    ]
  },
  "strip-plist":[
	  "/usr/share/examples/pc",
	  "/usr/share/examples/ppp"
  ]
}
```

### iso
The "iso" target within the manifest controls all the options specific to creation/setup of the ISO image. This can involve setting a custom install script, choosing packages which need to be installed or available for installation on the ISO, and more.

#### ISO Options
* "file-name" (string): Template for the generation of the ISO filename. There are a few format options which can be auto-populated:
   * "%%TRUEOS_VERSION%%" : Replace this field with the value of the TRUEOS_VERSION environment variable.
   * "%%GITHASH%%" : (Requires sources to be cloned with git) Replace this field with the hash of the latest git commit.
   * "%%DATE%%" : Replace this field with the date that the ISO was generated (YYYYMMDD)'
* "install-script" (string): Tool to automatically launch when booting the ISO (default: `pc-sysinstaller`)
* "auto-install-script" (string): Path to config file for `pc-sysinstall` to perform an unattended installation.
* "post-install-commands" (JSON array of objects) : Additional commands to run after an installation with pc-sysinstaller (not used for custom install scripts).
   * "chroot" (boolian) : Run command within the newly-installed system (true) or on the ISO itself (false)
   * "command" (string) : Command to run
* "prune" (JSON object) : Lists of files or directories to remove from the ISO
   * "default" (JSON array of strings) : Default list (required)
   * "ENV_VARIABLE" (JSON array of strings) : Additional list to be added to the "default" list **if** an environment variable with the same name exists.
* "dist-packages" (JSON object) : Lists of packages (by port origin) to have available in .txz form on the ISO
   * "default" (JSON array of strings) : Default list (required)
   * "ENV_VARIABLE" (JSON array of strings) : Additional list to be added to the "default" list **if** an environment variable with the same name exists.
* "offline-update" (boolian) : If set to true will generate a system-update.img file containing ISOs dist files
* "optional-dist-packages" (JSON object) : Lists of packages (by port origin) to have available in .txz form on the ISO. These ones are considered "optional" and may or may not be included depending on whether the package built successfully.
   * "default" (JSON array of strings) : Default list (required)
   * "ENV_VARIABLE" (JSON array of strings) : Additional list to be added to the "default" list **if** an environment variable with the same name exists.
* "pool" (JSON object) : Settings for boot pool
 * "name" (string) : Default name of ZFS boot pool
* "prune-dist-packages" (JSON object) : Lists of *regular expressions* to use to find and remove dist packages. This is useful for forcibly removing particular types of base packages.
   * Note: The regular expression support is shell based (grep -E "expression"). Lookahead and look
   * "default" (JSON array of strings) : Default list (required)
   * "ENV_VARIABLE" (JSON array of strings) : Additional list to be added to the "default" list **if** an environment variable with the same name exists.
* "iso-packages" (JSON object) : Lists of packages (by port origin) to install into the ISO (when booting the ISO, these packages will be available to use)
   * "default" (JSON array of strings) : Default list (required)
   * "ENV_VARIABLE" (JSON array of strings) : Additional list to be added to the "default" list **if** an environment variable with the same name exists.
* "ignore-base-packages" (JSON array of strings) : List of base packages to ignore when installing base packages into the ISO. 
   * This is turned into a regex automatically, so "-clang-" will remove all forms of the clang package, but "-clang-development" will only ignore the development package for clang.
   * **WARNING** Do *NOT* ignore the "runtime" package - this will typically break the ability of the ISO to start up.
* "auto-install-packages" (JSON object) : Lists of packages (by port origin) to automatically install when using the default TrueOS installer.
   * **NOTE:** These packages will automatically get added to the "dist-packages" available on the ISO as well.
   * "default" (JSON array of strings) : Default list (required)
   * "ENV_VARIABLE" (JSON array of strings) : Additional list to be added to the "default" list **if** an environment variable with the same name exists.
* "overlay" (JSON object) : Overlay files or directories to be inserted into the ISO
   * "type" (string) : One of the following options: [git, svn, tar, local]
   * "branch" (string) : Branch of the repository to fetch (svn/git).
   * "url" (string) : Url to the repository (svn/git), URL to fetch tar file (tar), or path to the directory (local)
   
#### ISO Example
```
"iso" : {
  "file-name": "TrueOS-x64-%%TRUEOS_VERSION%%-%%GITHASH%%-%%DATE%%",
  "install-script" : "/usr/local/bin/my-installer",
  "auto-install-script" : "",
  "post-install-commands": [
      {
        "chroot": true,
        "command": "touch /root/inside-chroot"
      },
      {
        "chroot": false,
        "command": "touch /root/outside-chroot"
      },
      {
        "chroot": true,
        "command": "rm /root/outside-chroot"
      },
      {
        "chroot": false,
        "command": "rm /root/inside-chroot"
      }
  ],
  "prune": {
    "ENV_VARIABLE": [
      "/usr/share/examples",
      "/usr/include"
    ],
    "default": [
      "/usr/local/share/examples",
      "/usr/local/include"
    ]
  },
  "ignore-base-packages": [
    "-clang-",
    "-sendmail-"
  ],
  "iso-packages": {
    "default": [
      "sysutils/ipmitool",
      "sysutils/dmidecode",
      "sysutils/tmux"
    ],
    "ENV_VARIABLE": [
      "archivers/cabextract"
    ]
  },
  "dist-packages": {
    "default": [
      "sysutils/ipmitool",
      "sysutils/dmidecode",
      "sysutils/tmux"
    ],
    "ENV_VARIABLE": [
      "archivers/cabextract"
    ]
  },
  "auto-install-packages": {
    "default": [
      "sysutils/ipmitool",
      "sysutils/dmidecode",
      "sysutils/tmux"
    ],
    "ENV_VARIABLE": [
      "archivers/cabextract"
    ]
  },
  "overlay": {
    "type": "git",
    "branch": "master",
    "url": "https://github.com/trueos/iso-overlay"
  }
}
```

### ports
The "ports" target allows for configuring the build targets and options for the ports system. That can include changing the default version for particular packages, selecting a subset of packages to build, and more.

#### Ports Options
* "type" (string) : One of the following: [git, svn, tar, local, null]. Where to look for the ports tree.
* "branch" (string) : Branch of the repository to use (svn/git only)
* "url" (string) : URL to the repository (svn/git), where to fetch the tar file (tar), or path to directory (local)
* "local_source" (string) : Path to a local directory where the ports tree should be placed (used for reproducible builds). This directory name will be visible in the output of `uname` on installed systems.
* "build-all" (boolian) : Build the entire ports collection (true/false)
* "build" (JSON object) : Lists of packages (by port origin) to build. If "build-all" is true, then this list will be treated as "essential" packages and if any of them fail to build properly then the entire build will be flagged as a failure.
   * "default" (JSON array of strings) : Default list (required)
   * "ENV_VARIABLE" (JSON array of strings) : Additional list to be added to the "default" list **if** an environment variable with the same name is set
* "make.conf" (JSON object) : Lists of build flags for use when building the ports.
   * "default" (JSON array of strings) : Default list (required)
   * "ENV_VARIABLE" (JSON array of strings) : Additional list to be added to the "default" list **if** an environment variable with the same name is set
* "strip-plist" (JSON array of strings) : List of files or directories to remove from any packages that try to use them.

#### Ports Example
```
"ports" : {
  "type" : "git",
  "branch" : "trueos-master",
  "url" : "https://github.com/trueos/trueos-ports",
  "local_source" : "/usr/ports",
  "build-all" : false,
  "build" : {
    "default" : [
      "sysutils/tmux",
      "shells/zsh",
      "shells/fish"
    ],
    "ENV_VARIABLE" : [
      "shells/bash"
    ]
  },
  "make.conf" : {
    "default" : [
      "shells_zsh_SET=STATIC",
      "shells_zsh_UNSET=EXAMPLES"
    ],
    "ENV_VARIABLE" : [
      "shells_bash_SET=STATIC"
    ]
  },
  "strip-plist":[
	  "/usr/local/share/doc/tmux",
	  "/usr/local/share/examples/tmux"
  ]
}
```

### poudriere-conf
This field contains a list of options to use to configure the poudriere instance that will build the packages. The configuration of poudriere is automatically performed to ensure an optimal result for most build systems, but it is possible to further customize these settings as needed.

#### Poudriere-conf Options
* "poudriere-conf" (JSON array of strings) : List of configuration options for poudriere
* */etc/poudriere.conf.release* - If this file exists on the system, it will be appended as a whole to the auto-generated config.

Common options to configure:

* "NOHANG_TIME=[number]" : Number of seconds a port build can be "silent" before poudriere stops the build.
* "PREPARE_PARALLEL_JOBS=[number]" : Number of CPUs to use when setting up poudriere (typical setting: Number of CPU's - 1)
* "PARALLEL_JOBS=[number]" : Number of ports to build at any given time.
* "ALLOW_MAKE_JOBS=[yes/no]" : Allow ports to build with more than one CPU. 
   * *WARNING:* Make sure to set the "MAKE_JOBS_NUMBER_LIMIT=[number]" in the builds -> make.conf settings to restrict ports to a particular number of CPUs as well.
* USE_TMPFS=[all, yes, wrkdir, data, localbase, no]" : Set how much of the port builds should be performed in memory.

#### Poudriere-conf Example
```
"poudriere-conf": [
	"NOHANG_TIME=14400",
	"PREPARE_PARALLEL_JOBS=15",
	"PARALLEL_JOBS=3",
	"USE_TMPFS='yes'",
	"ALLOW_MAKE_JOBS=yes"
]
```

An example of the automatically-generated config file is included below for reference (if nothing is supplied via the JSON manifest):
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

### pkg-repo
As part of the build process, the packages can also be automatically assembled into a full package repository which may be used for providing access to the newly-build packages on other systems.

#### pkg-repo Options
* "pkg-repo-name" (string) : Short-name for the package repository (default: "TrueOS")
* "pkg-train-name" (string) : Name for the package repository train used by sysutils/sysup (default: "TrueOS")
* "pkg-repo" (JSON Object) : Settings for the unified base+ports package repo
   * "url" (string) : Public URL where the repository can be found. (Distro creators will need to setup access for this URL and copy the pkg repo files as needed to make them available at the given location).
   * "pubKey" (JSON Array of strings) : SSL public key to use when verifying integrity of downloaded packages (one line of test per item in the array). This is basically just the plain-text of the SSL public key file converted into an array of strings. 
      * **WARNING** Make sure that this public key is the complement to the private key that you are using to sign the packages!!
   
#### pkg-repo Example

```
"pkg-repo-name" : "TrueOS",
"pkg-train-name" : "snapshot",
"pkg-repo" : {
  "url" : "http://pkg.trueos.org/pkg/release/${ABI}/latest",
  "pubKey : [
    "-----BEGIN PUBLIC KEY-----",
    "sdigosbhdgiub+asdgilpubLIUYASVBfiGULiughlBHJljib"
    "-----END PUBLIC KEY-----"
  ]
}
```
