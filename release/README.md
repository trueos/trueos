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

* ports - JSON Array - Set array elements to location and other settings of ports repo

```
  "ports":{
    "type":"git",
    "url":"https://github.com/trueos/trueos-ports",
    "branch":"trueos-master",
    "local_source":"/usr/local_source",
    "make.conf":[
      "devel_git_SET=SVN"
    ]
  }
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
* "dist-packages-are-essential" (boolian) : (default: true) Include the list of dist-packages in the essential package checks for verifying if a build was successful.
* "offline-update" (boolian) : If set to true will generate a system-update.img file containing ISOs dist files
* "optional-dist-packages" (JSON object) : Lists of packages (by port origin) to have available in .txz form on the ISO. These ones are considered "optional" and may or may not be included depending on whether the package built successfully.
   * "default" (JSON array of strings) : Default list (required)
   * "ENV_VARIABLE" (JSON array of strings) : Additional list to be added to the "default" list **if** an environment variable with the same name exists.
* "iso-packages" (JSON object) : Lists of packages (by port origin) to install into the ISO (when booting the ISO, these packages will be available to use)
   * "default" (JSON array of strings) : Default list (required)
   * "ENV_VARIABLE" (JSON array of strings) : Additional list to be added to the "default" list **if** an environment variable with the same name exists.
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
* iso-overlay - JSON Array, Used to list locations overlay directory to install ISO

```
  "iso-overlay":{
    "type":"git",
    "url":"https://github.com/trueos/iso-overlay",
    "branch":"https://github.com/trueos/iso-overlay"
  }
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

* world-flags - Allows setting different make options for building world directly in manifest

```
  "base-packages": {
          "world-flags":{
                "default": [
                        "WITH_FOO=1",
                        "WITH_BAR=2"
                ]

          },
  },
```

* kernel-flags - Allows setting different make options for building kernel directly in manifest

```
  "base-packages": {
          "world-flags":{
                "default": [
                        "WITH_FOO=1",
                        "WITH_BAR=2"
                ]

          },
  },
```




* And More! Take a look at [default manifest](trueos-manifest.json) for details.

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

### pkg-repo
As part of the build process, the packages can also be automatically assembled into a full package repository which may be used for providing access to the newly-build packages on other systems.

#### pkg-repo Options
* "pkg-repo-name" (string) : Short-name for the package repository (default: "TrueOS")
* "pkg-repo" (JSON Object) : Settings for the unified base+ports package repo
   * "url" (string) : Public URL where the repository can be found. (Distro creators will need to setup access for this URL and copy the pkg repo files as needed to make them available at the given location).
   * "pubKey" (JSON Array of strings) : SSL public key to use when verifying integrity of downloaded packages (one line of test per item in the array). This is basically just the plain-text of the SSL public key file converted into an array of strings. 
      * **WARNING** Make sure that this public key is the complement to the private key that you are using to sign the packages!!
   
#### pkg-repo Example

```
"pkg-repo-name" : "TrueOS",
"pkg-repo" : {
  "url" : "http://pkg.trueos.org/pkg/release/${ABI}/latest",
  "pubKey : [
    "-----BEGIN PUBLIC KEY-----",
    "sdigosbhdgiub+asdgilpubLIUYASVBfiGULiughlBHJljib"
    "-----END PUBLIC KEY-----"
  ]
}
```
