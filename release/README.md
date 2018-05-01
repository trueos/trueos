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

Aditional Configuration:
--------------

* /etc/poudriere.conf.release - If this file exists, it will be included with the poudriere build process, allowing systems to override specific settings such as TMPFS, or MAKE_JOBS that are more suitable for that builder.

Build Instructions:
--------------
The following instructions may be used to generate TrueOS installation
images:

```
make buildworld buildkernel
make packages
cd release && make poudriere && make release
```
