# wacom-distortion
Calibration tool for wacom stylus. To use this calibration tool you need to patch the package `xf86-input-wacom`, instruction to do this are given at the end of this document.

### Compilation

    qmake && make
### Execution
You need the name of your stylus device that you can find with the command `xinput`

    ./wacom-distortion <device>
### Dependencies

    xinput, qt5, c++11

# wacom driver
### Download sources

    apt-get source xf86-input-wacom
### Patch sources
`cd` in the sources directory that you just downloaded.
With the help of the file `distortion.patch` (which is in this repository) patch the sources :

    patch -p0 < /path/to/distortion.patch
### Compile package

    sudo apt-get install devscripts build-essential lintian
    dpkg-buildpackage -us -uc
### Install package

    sudo dpkg -i ../xserver-xorg-input-wacom_???_???.deb
