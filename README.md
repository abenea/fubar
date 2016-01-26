# fubar

fubar is a Qt media player. It has the following features:

* updating the playlist via inotify
* cue sheets
* global shortcuts
* last.fm integration


## Dependencies

fubar requires:

* Qt >= 4.8
* kdelibs (for global shortcuts)
* phonon
* boost
* liblastfm >= 1.0.7
* [TagLib](http://taglib.github.com/) >= 1.8
* cmake >= 2.6
* libmpv >= 0.8.0
* libcue


## Compiling fubar

    mkdir build
    cd build
    cmake ..
    make -j4

You can run `build/fubar` directly or install it using `make install`.


## Using fubar

To watch a directory with music, add it from Preferences->Library.


## License

fubar is released under the [MIT License](http://www.opensource.org/licenses/MIT).
