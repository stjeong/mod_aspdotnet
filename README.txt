README for mod_aspdotnet + Apache.Web 
-----------------------------------

mod_aspdotnet.so and Apache.Web.dll require Windows 2000 or later, they
are not maintained to be compatible with Windows 98, ME or NT 4.  Your
results attempting to use earlier flavors will vary.


Build Notes
-----------

The top level workspace mod_aspdotnet.sln is invoked for a release build
for Apache 2.0 with the syntax;

  set APACHE_PATH=d:\path\to\apache2
  devenv mod_aspdotnet.sln /useenv /build Release /project mod_aspdotnet

or to build for Apache 2.2, invoke with the syntax;

  set APACHE22_PATH=d:\path\to\apache2.2
  devenv mod_aspdotnet.sln /useenv /build "Release 2.2" /project mod_aspdotnet

The necessary files, mod_aspdotnet.so [+.pdb for debug symbols] and the
Apache.Web.dll [+.pdb] are built into the installer.msi package at 
the top-level Release/ directory of this module.  

The true "Debug" and "Debug 2.2" flavors work similarly, and eliminate 
optimizations that make debugging difficult.  However, the installer is
not designed to package the debug flavor into an .msi package.  See the
"Deployment Notes" section later in this README for an explanation of
how to manually install either flavor, locally, without an .msi package.

It is absolutely critical that this build occurs using the Visual Studio
.NET 2005 (al la 8.0) version.  Older VS 7.x and earlier compilers won't 
support the C++ CLR syntax used for this module, and newer VS compilers 
may again change the CLR C++ syntax to be entirely incompatible with the
sources.

The Apache.Web.dll is no longer temporarily registered, it's type library
(.tlb file) is extracted using tlbexp.  Earlier notes about this should be
ignored.

We build Apache.Web.dll with delay-load of libhttpd, libapr and libaprutil 
dll's.  This is important since Apache.Web.dll normally can't be loaded,
even for the .NET framework tools to inspect the .dll, unless it is literally 
adjacent (in the same directory) to Apache's .dll's.  And Apache.Web.dll must
be in the Global Assembly Cache to participate in the system.web framework.
So, using this alternate delay-load method, Apache.Web.dll can be loaded 
(provided we don't instantiate an Apache.Web.WorkerRequest object), even when
it cannot resolve the path to Apache's dll's in the Serverroot/bin directory.

Before using InstallShield to actually package a release build, the syntax;

  devenv mod_aspdotnet.sln /useenv /build Release /project resolve_apache
  devenv mod_aspdotnet.sln /useenv /build "Release 2.2" /project resolve_apache

will build the helper .dll to allow the installer to search the local machine
for installations of Apache.exe (Release flavor) or httpd.exe (Release 2.2).


Deployment Notes
----------------

YOU should test that the module runs under both the full .NET Framework SDK, 
as well as within the .NET Framework Runtime environment only.  A number 
of internal behaviors vary between these two environments, and Exceptions,
destruction, construction and failure cases will manifest differently between
the two environments.

ASP.NET requires the Apache::Web::Request object to be registered in 
the Global Assembly Cache in order to instantiate it's host container.
So it is not possible to install Apache.Web.dll in the 'usual' modules
subdirectory of Apache HTTP Server.

In order to install a reference into the Global Assembly Cache (for testing
the release build or debug build) use;

  regasm d:\path-to-apache\bin\Apache.Web.dll 
  gacutil /ir Apache.Web.dll FILEPATH d:\path-to-apache\bin\Apache.Web.dll Apache.Web 

This installs Apache.Web.dll by reference to the built assembly.  Note that
UAC will prevent you from doing this in Vista or 2008 and you may need to
launch a command shell "run as Administrator" in order to complete this
package registration in the global cache.

This may *NOT* work on an end-user's machine without the .NET Platform SDK.  
Those tools [regasm/gacutil] were not officially going to be distributed
with the retail .NET runtime redistributables.  Instead, the .msi installer, 
has all details for registering the Apache.Web assembly into the Global 
Assembly Cache, is entrusted to install Apache.Web to the GAC for end-users.
Note that it appears they are installed with the runtime, but all warnings
by Microsoft were to the contrary.


TODO
----

Consider pre-compiling the package into native code prior to distribution.
(Apparently, InstallShield always does this for GAC-registered modules, so
this consideration might be moot.)

Consider building a roll-up assembly integrating the libhttpd et. al. with
the Apache.Web.dll package.  Perhaps we can force them to live in different
locations with this method.  Note roll up assemblies -reference- the other
.dll's [that assembly is a .dll itself] but it does not actually merge the
other .dll's into a single package.

Note that the .msi package searches for apache/bin/apache.exe up to 3 levels 
deep from the local hard drives, in sequence.  It aught to test further, as
necessary, or allow explicitly long paths, if the user chooses.  The path can
be manually specified in the installer, though, so the issue is not fatal,
and the 3 level depth avoids waiting a half hour when invoking the installer.
