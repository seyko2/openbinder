/*!
@page MissingFeatures Missing Features

<div class="header">
<center>\< @ref Authors | @ref OpenBinder | @ref BringItOn \></center>
<hr>
</div>

This page provides information about major missing features
from OpenBinder that most people will probably be interested in.  For a
full list of all things that need to be done, see the
@ref todo "cannonical to-do list".

@section BinderShellEditing No Command Line Editing and History in Binder Shell
@todo Implement command line history and editing in Binder Shell.

One of the most annoying limitations right now is that the Binder
shell doesn't have any support for command line editing.

Unfortunately implementing this probably isn't as easy as just taking an existing package
like readline, because it needs to sit on top of the binder IO streams (IByteInput
and IByteOutput) instead of raw system calls.  A quick and dirty hack may be to
add an API to get the tty for a stream that the package can then use if available...
though there would be some nasty issues to deal with there around cross-process operation.

A more complete design is probably to add a couple new interfaces:

- ITTY is an interface to a TTY device, providing things like information
  about the terminal dimensions, kind of device, etc.  This would be a fairly
  direct mapping to a traditional tty device.
- ITerminal is a higher-level interface for operating on a terminal device,
  with APIs like "MoveCursor()" and "DeleteLine()".  This is the API the
  Binder Shell would use (if available) to implement command line editing.
  The normal output streams such as "bout" would also implement the ITTY
  interface, and the Binder Shell would create a standard translator object
  that provides an ITerminal on top of an ITTY+IByteOutput.  Alternatively,
  a graphical terminal could supply its own ITerminal implementation without]
  having to deal with terminal command sequences.

@section BinderShellHostAccess Binder Shell Doesn't Provide Host Access
@todo Implement Binder Shell access to underlying host commands / filesystem.

Currently the Binder Shell doesn't have facilities for directly running
host commands or accessing the host filesystem.

For host commands, this is mostly a matter if adding some code to
SyntaxTree.cpp:SimpleCommand that, as a last resort, tries to find the command
in $PATH and does a fork()/exec() to run it.  The only tricky thing here is
setting up the forked process's streams correctly for the current shell
environment.  (Ideally, we would want to have a way to get the underlying
file descriptors of the current shell's IByteInput/IByteOutput to hand those
off to the forked process...  though it also needs to deal with the situation
where those are not sitting on top of a file descriptor.)

For the host filesystem, this should probably be done by implementing a
@ref BinderDataModel layer on top of the filesystem.  The main issues to
deal with here are to decide how the Binder and host namespaces are merged,
and an efficient way to transfer filesystem references between processes
without doing an excessive amount of IPC.  Ideally, it would be cool if
we could transfer raw file descriptors between processes somewhere like
Binder objects...

@section NoNetworkSupport No Network Support
@todo Implement cross-network communicatiom support

While the Binder is a distributed component model, communication across
networks has not yet been implemented.  This is because the focus of
its development has been on system-level services, where communication
across processes is the key feature.  Adding network support should
be relatively straight-forward, using the facilities such as
marshalling/unmarshalling that are already present.

The main issue will probably be in dealing with object identities, such
as deciding how/if to deal with the case of an object reference being
transfered from host A, to host B, to host C, and back to A.

@section NoObjectIntrospection No Object Introspection
@todo Implement object introspection

The Binder object API (IBinder) provides a facility for finding the
interfaces that an object implements, but there is currently no API to
programmatically discover the methods, properties, and events on those
interfaces.  This should be fairly easy to add by defining a new API
on IBinder to retrieve this information (we want this tied to the IBinder
and not the interface because of the scripting nature of the binder
protocol), and then with little work pidgen can use all of the information
it is already generating to implement it.

*/
