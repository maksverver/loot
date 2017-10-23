Loot is a simple GUI tool to manage resources which can be opened/closed.

For example, you can use this to easily mount/unmount network shares. Each
resource, called a "box", can be open or closed.

Boxes are configured by writing shell scripts (or other executables) that
perform some action when they are executed with a single command argument. The
boxes must be stored in the ${XDG_CONFIG_HOME}/loot/ directory (or
${HOME}/.config/loot/, by default).

The supported actions are the following:

  status
  The script should write a single line of output: "opened" or "closed".

  open
  The script should open the resource.

  close
  The script should close the resource.

That's it. Loot will only call "open" if the state is closed, and vice versa.
If the script returns an error, the box is considered to be in an error state.
See examples/dummy.sh for an example script.

To integrate with the desktop environment, copy the binary to a location in
your path, and then copy examples/loot.desktop to ~/.local/share/applications.

To start loot automatically at startup, copy the desktop entry to
~/.config/autostart/
