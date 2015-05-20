/*!
@mainpage OpenBinder
@anchor OpenBinder

OpenBinder is a system-level component architecture, designed to provide a
richer high-level abstraction on top of traditional modern operating system
services.  The current implementation runs on Linux, but the code has run
on a diverse variety of platforms, including BeOS, Windows, and PalmOS Cobalt.

To get started as quickly as possible, you can jump immediately to the
@ref QuickStart section.

See @ref BinderOverview if you just want to know what this Binder thing is.

@section GettingStarted Getting Started

This is basic information about the OpenBinder distribution and how to
use it.

- @subpage License "License": The license this code is released under.
- @subpage QuickStart "Quick Start": Quickly get your feet wet.
- @subpage Building "Building": Details on how to build the source distribution.
- @subpage Contents "Contents": Overview of the contents of this distribution. 
- @subpage Authors "Authors": Who has contributed to the code.
- @subpage MissingFeatures "Missing Features": The major things you'll notice.
- @subpage BringItOn "Bring It On": The smooved manifesto.

@section ProgrammingIntroduction Programming Introduction

These pages provide an overview of the Binder and its capabilities.

- @subpage BinderOverview "Binder Overview": High-level description of OpenBinder.
- @subpage APIConventions "API Conventions": Typographic conventions we use in our code.
- @subpage BinderThreading "Threading Conventions": How multithreading is used in the Binder.
- @subpage EnvironmentVars "Environment Variables": Run-time control and customization of the Binder.

@section ProgrammingGuide Programming Guide

The Binder architecture is divided into a number of separate subsystems, generally
building on top of each other.  Note that this documentation is divided into a
set of "kits" that is cleaner than how the current source tree is organized.

- @subpage SupportKit "Support Kit": a set of standard tools and APIs
  used to write Binder code.  This includes basic types such as strings,
  container classes such as vectors, tools for reference counting and memory
  management, and threading utilities.
  - @ref SAtomDebugging
  - @ref SHandlerPatterns
- @subpage BinderKit "Binder Kit": the core Binder APIs, defining Binder objects,
  interfaces, and other concepts.
  - @ref BinderRecipes
  - @ref BinderShellTutorial
    - @ref BinderShellData
    - @ref BinderShellSyntax
  - @ref BinderTerminology
  - @ref BinderProcessModel
    - @ref BinderIPCMechanism
  - @ref pidgen
  - @ref BinderInspect
- @subpage StorageKit "Storage Kit": APIs for accessing and manipulating data.
  - @ref BinderDataModel
  - @ref WritingDataObjects

See @ref CoreSupport "Support Kit Reference" for a detailed listing of the Binder and
related APIs.
*/
