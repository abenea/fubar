# fubar

fubar is a native Linux media player meant for foobar2000 fanatics.
Currently, it doesn't implement too many of the features of its
idol, but it looks awesome(only in KDE :) ).

# Dependencies/Installation

fubar requires:

* Qt >= 4.8
* liblastfm >= 1.0.7
* [TagLib](http://taglib.github.com/) >= 1.8
* cmake

## Debian Wheezy

We need the libtag1-dev and liblastfm-dev from experimental, so add to /etc/apt/sources.list:

    deb http://ftp.debian.org/debian/ experimental main non-free contrib

Run:

    sudo apt-get install cmake g++ libboost-all-dev protobuf-compiler libprotobuf-dev libqt4-dev libphonon-dev kdelibs5-dev phonon-backend-gstreamer gstreamer0.10-plugins-ugly
    sudo apt-get install -t experimental libtag1-dev liblastfm-dev

## Other Distributions

### Compiling liblastfm

Dependencies:

* FFTW http://www.fftw.org/
* libsamplerate http://www.mega-nerd.com/SRC/

    apt-get install libsamplerate0-dev libfftw3-dev
    git clone git://github.com/bugdone/liblastfm.git; cd liblastfm ; mkdir build ; cd build ; cmake ..
    make -j4 ; sudo make install ; cd ../..

# Compiling fubar

    git clone https://yoyosan@bitbucket.org/bugdone/fubar.git ; cd fubar; mkdir build ; cd build ; cmake ..
    make -j4

At the moment, no installation is required. Just add a symlink to one of the directories in $PATH:

    CURRENT="`pwd`"; sudo ln -s "$CURRENT/fubar" /usr/local/bin/fubar


# Using fubar

To watch a directory with music, add it from Preferences->Library.

Enjoy.
