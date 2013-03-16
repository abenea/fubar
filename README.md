# fubar

fubar is a native Linux media player meant for foobar2000 fanatics.
Currently, it doesn't implement too many of the features of its 
idol, but it looks awesome(only in KDE :) ).

# Dependencies/Installation

fubar requires:

* Qt 4.8
* modified liblastfm (git://github.com/bugdone/liblastfm.git)
* FFTW http://www.fftw.org/
* libsamplerate http://www.mega-nerd.com/SRC/
* cmake

## Debian Wheezy

Before cloning the project, the following need to be done:

Run:

        sudo apt-get install libboost-all-dev protobuf-compiler libprotobuf-dev libqt4-dev libphonon-dev kdelibs5-dev phonon-backend-gstreamer gstreamer0.10-plugins-ugly cmake libfftw3-dev libsamplerate0-dev

You need the latest version of libtag1-dev(1.8.1) so add the experimental repo in your /etc/apt/sources.list, besides your
unstable one:

         deb http://ftp.ro.debian.org/debian/ unstable main non-free contrib
         deb http://ftp.ro.debian.org/debian/ experimental main non-free contrib

Save and run:

     sudo apt-get update ; sudo apt-get install libtag1-dev -t experimental

### Installing liblastfm

Clone the modified liblastfm dependency, in whatever directory you wish:

      git clone git://github.com/bugdone/liblastfm.git ; cd liblastfm ; mkdir build ; cd build ; cmake ..
      make -j4 ; sudo make install ; cd ../..

### "Installing" fubar

    git clone https://yoyosan@bitbucket.org/bugdone/fubar.git ; cd fubar; mkdir build ; cd build ; cmake ..
    make -j4

At the moment, no installation is required. Just add a symlink to one of the directories in $PATH:

       CURRENT="`pwd`"; sudo ln -s "$CURRENT/fubar" /usr/local/bin/fubar
   

### Using fubar

To watch a directory with music, add it from Preferences->Library.

Enjoy.
