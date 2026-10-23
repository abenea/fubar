# fubar

fubar is a Qt media player. It has the following features:

* updating the playlist via inotify
* cue sheets
* global shortcuts
* last.fm integration
* [Navidrome](https://www.navidrome.org/) (Subsonic API) streaming


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

## Navidrome

fubar can play the music on a [Navidrome](https://www.navidrome.org/) server (or any server
speaking the Subsonic API).

* Preferences->Navidrome... sets the server URL, username and password. "Test connection"
  checks them before saving. Leave the URL empty to turn the integration off.
* File->New Navidrome library view opens a read-only tab with the whole server library, which
  you can filter (Ctrl+J), play and queue like the local library. It is cached on disk, checked
  for changes at startup and every hour, and File->Refresh Navidrome library syncs it on demand.
* Played tracks are reported to Navidrome (play counts, scrobbling). If Navidrome forwards
  scrobbles to Last.fm, disable either this option or fubar's Last.fm plugin to avoid duplicates.
* To keep Navidrome tabs across restarts, enable the `mainwindow.save_tabs` option in
  Preferences->Config.

fubar does not store your password. It keeps a Subsonic salt and token in its config file
(`~/.config/fubar/fubar.conf`); the token gives access to that server like the password does,
and only changing the Navidrome password revokes it.

## License

fubar is released under the [MIT License](http://www.opensource.org/licenses/MIT).
