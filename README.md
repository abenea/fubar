# fubar

fubar is a Qt media player. It has the following features:

* updating the playlist via inotify
* cue sheets
* global shortcuts
* last.fm integration


## Dependencies

fubar requires:

* Qt >= 5
* kdelibs (for global shortcuts)
* boost
* liblastfm >= 1.0.7
* [TagLib](http://taglib.github.com/) >= 1.8
* cmake >= 3.13
* libmpv >= 0.8.0
* libcue
* googletest


## Compiling fubar

    cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo -B build
    cmake --build build -j16

You can run `build/fubar` directly or install it using `make install`.

## Using fubar

To watch a directory with music, add it from Preferences->Library.

### Wayland

If running under KWin>=6 and Wayland, optionally install the KWin script to
create a global shortcut for Show/Hide (configure it under System Settings >
Shortcuts > Window Management > Show/Hide fubar):

    kpackagetool6 --type=KWin/Script --install kwin/fubar
    kwriteconfig6 --file kwinrc --group Plugins --key fubarEnabled true
    qdbus6 org.kde.KWin /KWin reconfigure

## License

fubar is released under the [MIT License](http://www.opensource.org/licenses/MIT).
