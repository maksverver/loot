Loot is a simple GUI tool to manage resources which can be opened and closed.

![Screenshot](https://raw.githubusercontent.com/maksverver/loot/master/artwork/screenshot.png)

For example, you can use this to easily mount/unmount network shares.

Loot is visible as a status icon in the notification area. When clicked, it pops
up a list of available boxes, and shows their state (open, closed, or *error* if
the last script execution failed). Simply click a box to change its state. If it
was open, it will be closed. If it was closed, it will be opened. If it was in
an error state, Loot will try to refresh its state.

The Loot status icon shows a summary of the state of all boxes. It is closed if
all boxes are closed, open if at least one box is open and all others are
closed, or in error state if at least one box is in an error state.

The boxes themselves are configured by adding shell scripts (or other
executables) to the configuration directory, at `${XDG_CONFIG_HOME}/loot/` (or
`${HOME}/.config/loot/`, by default).

## Box protocol

A box is an executable that takes a single argument: the action to be executed.
Each box manages a single resource. The supported actions are the following:

  * `status`

    The script should write a single line of output describing the state of the
    resource: `opened` or `closed`. (Any other output is considered an error.)

  * `open`

    The script should open the resource.

  * `close`

     The script should close the resource.

That's it. Loot will only call "open" if the state is closed, and vice versa.
If the script exits with an error status, the box is considered to be in an
error state. See `examples/dummy.sh` for an example script.

## Desktop integration

There is a sample desktop entry in `examples/loot.desktop`. To integrate with
your desktop environment, copy the `loot` binary to a location in your path, and
then copy `loot.desktop` to (e.g.) `~/.local/share/applications`.

To start loot automatically upon login, copy (or symlink/hardlink) the desktop
entry to `~/.config/autostart/`.

## Packaging

There is a `PKGBUILD` file which allows building Loot as an Arch Linux package.
To install, simply run:

```sh
makepkg -fc
sudo pacman -U loot-*.pkg.tar.xz 
```

## Dependencies

Loot is written in C and depends on GTK+ 3. It runs on Linux and maybe on other
POSIX systems, too.
