This sample demonstrates writing a Binder component.  The component
is implementing the ICommand interface, so that it can be used as
a shell command.  (All shell commands are implemented as components.)

To see it in action, be sure the component package is available (the
OpenBinder build system will do this for you) and type "sample_component" in the shell.

Be sure if you are going to write some real code derived from this
example that you change the package name in the Makefile to be a name
in your own namespace.

If you want to write something besides a command, you will want to
change the interface the component implements and update the
Manifest.xml file appropriately.
