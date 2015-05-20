/*!
@page QuickStart Quick Start

<div class="header">
<center>\< @ref License | @ref OpenBinder | @ref Building \></center>
<hr>
</div>

This page will guide you through select parts of the OpenBinder
system and documentation to get started with it as quickly as possible,
and leave out all that wordy wordy stuff.

@section BuildingSec Building

Following the directions on the @ref Building page to get a working
build.  Note that until you want to play with multiple processes,
you can ignore the @ref MultiprocessBuild and following material.

@section CPlusPlus C++ Coding

For a quick look at what C++ coding with the Binder looks like,
you can go to the @ref BinderRecipes.  If you want to dig right
in and start coding, you can make a copy of SampleComponent and
begin by modifying that code.

@verbatim
cp -r samples/SampleComponent samples/MyComponent
@endverbatim

Be sure to change the Makefile to give your component its own
package and shell command name.  Once that is done, you should
be able to run smooved:

@verbatim
make runshell
@endverbatim

And from with the Binder Shell then run your component:

@verbatim
/# my_component
@endverbatim

The other sample code can be used for a similar purpose.

@section TheShell Binder Shell

Another way to start playing with the Binder is to go through the
@ref BinderShellTutorial.  This will show you a lot of useful things
you can do with the shell, while at the same time introducing you to
many of the key Binder concepts.

*/
