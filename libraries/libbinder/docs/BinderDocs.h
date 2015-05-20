/*!
@page BinderOverview Binder Overview

<div class="header">
<center>\< @ref BringItOn | @ref OpenBinder | @ref APIConventions \></center>
<hr>
</div>

The Binder defines and implements a distributed component architecture similar in broad
strokes to COM on Windows and CORBA on Unix. That is, the system is defined as a set of
abstract interfaces, operations on those interfaces, and implementations of them. As long
as you are using these interfaces, the actual object behind them can be written in any
language with Binder bindings, and exist anywhere &mdash; your own address space, in another
process, or even (eventually) on another machine.

@par
<b><em>Where did the name come from?</em></b>
@par
<em>The Binder derives its name from its common use as a backbone to which you can
attach a variety of separate pieces, creating a cohesive whole where the
individual pieces all fit together.</em>
@par
<em>This ability generally centers around the IBinder interface, the lingua franca of the
@ref BinderKit, but begins with the @ref SupportKit (providing
common threading and memory management facilities) and extends up to the
@ref BinderDataModel (a rich language for describing complex, active data
structures).</em>

The Binder is a distributed architecture, so you generally don't worry about processes or IPC
between them.  The Binder infrastructure takes care of making all objects look like they exist
in the local process, and dealing with IPC and other issues as needed.  It also takes care of
resource management between processes, so you are guaranteed that, as long as you have a
handle on an object in another process, that the object will continue to exist.  (Barring
catastrophic situations such as a process crashing, but allowing other processes to deal
gracefully with such the event.)

Unlike most other component architectures, the Binder is <i>system oriented</i> rather
than <i>application oriented</i>.  That is, the Binder is designed to support a new kind
of component-based system-level development, and the current focus of its distributed
architecture is on processes and IPC between them rather than cross-network
communication.  Indeed, network communication is not currently implemented, though that
is not because of limitations in the architecture: it simply has not been the focus of
development so far.

The Binder provides component/object representations of various basic system services,
such as processes (through IProcess) and shared memory (through IMemory), allowing you
to use them in new and more powerful ways.  Unlike many attempts at rethinking how a
system is designed, the Binder does not replace or hide these kinds of traditional
concepts in operating systems, but instead attempts to embrace and transcend
them.  This allows the full Binder programming model to sit on top of any traditional
operating system, and indeed in addition to Linux it has in the past run on BeOS,
Windows, and Palm OS Cobalt.

The Binder is mostly implemented in C++, and currently that is the language you will do
most of your programming in.  However the Binder Shell can serve as another Binder
language (you can even write Binder components with it), and because it exposes those
same Binder facilities in a scripting language it can be a good way to learn about basic
Binder concepts.  The @ref BinderShellTutorial provides such an introduction to the
shell and Binder.

There are also many rich facilities for doing multithreaded programming with the Binder,
though the Binder itself does not impose a particular threading model (in COM parlance,
it is purely free threaded),  It allows you to implement whatever threading model is
appropriate for your situation, though we do use various @ref BinderThreading in our code
built on top of the Binder.  In addition, calls from one Binder object to another are
always executed by having the thread in the first object simply hop over to the second. 
This model is extended to cross-process operations, providing the foundation for the
rich @ref BinderProcessModel.

The Binder has been used to implement a wide variety of commercial-quality system-level
services.  A primary example is the implemention of a distributed user interface
framework, where every node in the view hierarchy (including windows, layout managers,
and controls) is a Binder object, allowing them to be distributed across processes as
needed.  It has also been used to implement a rich media framework (where the media
graph is composed of Binder objects), an application manager, a font cache and font
engines (where font engines are components for easy replacement and allowing
applications to provide their own font engine of data for temporary use by the system),
power management services, a rich list view control that sits on top of the
@ref BinderDataModel as a data source, etc.

@section WhyBinder Why Binder?

The Binder architecture was developed to address specific issues that were encountered
while developing BeIA at Be, Inc and then Palm OS Cobalt at PalmSource, Inc.  Both of
these platforms were designed to run on small handheld or dedicated devices, an
environment that imposes some specific requirements on system software that are not
an issue in a more traditional desktop environment.

@subsection HardwareScalability Hardware Scalability

The mobile device world tends to have a much broader range of hardware capabilities, for
various reasons.  For example, size and battery life are extremely important issues, so
a new device may use less powerful hardware than an older device in order to meet these
goals.  This places a burden on system software, where one would like to be able to run
on anything from a 50MHz ARM 7 CPU (without memory protection) up to a 400MHz ARM 9 CPU
and beyond.

The Binder helps to address this situation by allowing for the creation of system
designs that have much more flexibility in how they use hardware. In particular, memory
protection and process communication is a significant overhead in modern operating
systems, so the Binder strongly supports system design that is not tied to process
organization.  Instead, the Binder can assign various parts of the system to processes
at run time, depending on the particular speed/size/stability/security trade-offs that
make sense.

@subsection SystemCustomization System Customization

Mobile and dedicated devices are unlike desktop machines in that, instead of the "one
size fits all" world of the desktop, their user experience and functionality can vary
widely between different devices. Both hardware manufacturers and phone carriers want to
deeply customize their behavior, partly to support their branding, but also because
these devices need to provide an experience that is more specific to the problems they
are trying to solve rather than having a general purpose user interface.

The Binder's component model, applied to system design, makes it much easier to support
this kind of customizability in a manageable way. The Palm OS Cobalt system architecture
takes advantage of this by including separable components for things like font engines,
key input handlers, power management, window management, and even UI focus indication. 
This has allowed, for example, a hardware vendor to customize how input focus is managed
on-screen without having to write fragile code that is deeply tied to the implementation
of the system software.

@subsection RobustApplications Robust Applications

The display and usage pattern of dedicated and mobile devices is also very different than
it is on desktops.  Desktop-style window management is generally not practical or, if
there is no touch screen, even impossible. Because of the limited amount of space on the
screen, the currently active application generally wants to consume as much of the
available space as possible, essentially becoming the user experience.

This usage of the screen can become a significant problem for more complicated
applications, such as web browsers.  Often a web browser will need to rely on various
third party code to display rich content such as movies, code that is itself quite
complicated and not under complete control of the browser.  If that code happens to
crash in a traditional system design, it will at the very least take with it the window
it is in &mdash; which on a mobile device usually means the entire user experience goes
away.

The Binder helps address this issue by making it easy for one application to sandbox
other parts of itself.  For example, a web browser using the Binder could decided to
create components for displaying movies and other complicated content in another
process, so that if those components crash they will not disrupt the containing browser
experience.  At the same time, the Binder can easily revert back to using a single
process (or only a few) depending on the capabilities of the hardware it is running on.

@section BinderComparedTo Binder Compared To...

Though the Binder is similar in broad strokes to most component-based systems,
the one it is most like is probably Microsoft's COM.  If you a familiar with
COM, many of the concepts used in the Binder will also be very familiar, though
the details can be quite different.  Since COM is the most popular component
system around, it makes sense to describe the Binder in terms of its
differences from COM.

- COM's baseline language is C, while the Binder's is C++.  By leveraging
  the richer C++ language, which has many more facilities to represent the
  object-oriented concepts of a component system, the Binder APIs can be
  much easier to use than the equivalent APIs in COM.  See
  @ref BinderRecipes for examples, such as the extensive use of smart pointer
  template classes to completely eliminate manual reference counting.

- COM's primary API is a static interface (IUnknown), while the Binder's
  core API is a dynamically typed protocol provided by IBinder.  In COM,
  scripting support is provided through a special interface (IDispatch)
  required to support scripting, while all Binder objects are intrinsically
  scriptable.  The @ref BinderShellTutorial contains many examples of how
  this can be used.  The static Binder interfaces used in C++ sit on top of that core
  scripting infrastructure, and are in fact completely optional.  Not only
  does this provide a powerful environment for dynamically typed languages,
  but it can also be used in C++ for situations where it is more convenient to use
  dynamic or ad-hoc bindings.  See, for example, BObserver.

- SValue, the Binder's equivalent to COM's "variant" dynamic type, includes
  a number of additional features.  It is not constrained to a fixed set of
  types, but simply operates on generic typed data blobs.  In addition, it
  can easily hold complex data structures (of sets of mappings of any
  SValue type, including other sets and mappings), with a fairly simple
  C++ API for dealing with these structures.
  
- In addition to events that can be generated from objects, the Binder also
  provides a simple dataflow-like mechanism for properties.  This allows
  you to connect the property of one object to the property of another object,
  such that a change in the first value will cause a corresponding change to
  the second.  This is useful, for example, to display dynamic data to the
  user such as a battery level.

- The Binder was designed from its beginning to be multithreaded.  It doesn't
  support (or impose) threading models like COM, but instead all objects are
  free threaded.  It includes many rich tools for threaded programming,
  including the lightweight SLocker class, a powerful predictive deadlock
  detection tool (@ref LOCK_DEBUG), and the SHandler class for scheduling
  timed messages out of the Binder's built-in thread pool.

*/
