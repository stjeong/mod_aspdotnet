/* Copyright 2007 Covalent Technologies
 *
 * Covalent Technologies licenses this file to You under the Apache
 * Software Foundation's Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// Apache.Web.Version.h Version declaration macros

// #define WIN32_LEAN_AND_MEAN

#ifndef APACHE_WEB_VERSION

// Borrow Versioning from httpd
#include "ap_release.h"

// The MAJOR.MINOR revision should reflect the Apache release this module 
// is built for.  The httpd release guidelines from version 2.0.43 onwards
// assure us that the binaries remain backwards compatibile, within each
// major.minor httpd release.  In the absense of Apache's numeric maj.min
// numeric values, we presume 2.0.
//
#ifdef AP_SERVER_MAJORVERSION_NUMBER
#define APACHE_WEB_VER_MAJOR AP_SERVER_MAJORVERSION_NUMBER
#define APACHE_WEB_VER_MINOR AP_SERVER_MINORVERSION_NUMBER
#else
#define APACHE_WEB_VER_MAJOR 2
#define APACHE_WEB_VER_MINOR 0
#endif

// The n.n.SUBVS reflects 0 for a binary stable (even) releases,
// or the httpd subversion for binary unstable (odd) dev releases.
//
#if (APACHE_WEB_VER_MAJOR % 2)
#define APACHE_WEB_VER_SUBVS AP_SERVER_PATCHLEVEL_NUMBER
#else
#define APACHE_WEB_VER_SUBVS 0
#endif

// The n.n.n.BUILD must be bumped every time Apache.Web class interfaces change.
// This ensures that a specific build of mod_aspdotnet loads it's correponding
// build of Apache.Web, and multiple builds of Apache.Web coexist in the GAC.
// This also ensures we use the most recent 'build' of the module for the given
// Apache core version.
//
#define APACHE_WEB_VER_BUILD 2199

// Define APACHE_WEB_VER_RELEASE as 1 for a non-dev, release build, when rolling
// the distribution .zip sources or binary release.
//
#define APACHE_WEB_VER_RELEASE 0

#define APACHE_WEB_VERSION_CSV \
    APACHE_WEB_VER_MAJOR,\
    APACHE_WEB_VER_MINOR,\
    APACHE_WEB_VER_SUBVS,\
    APACHE_WEB_VER_BUILD

// Properly quote a value as a string in the C preprocessor
#define APW_STRINGIFY(n) APW_STRINGIFY_HELPER(n)
// Helper macro for APR_STRINGIFY
#define APW_STRINGIFY_HELPER(n) #n

#define APACHE_WEB_VERSION \
    APW_STRINGIFY(APACHE_WEB_VER_MAJOR ##. \
                ##APACHE_WEB_VER_MINOR ##. \
                ##APACHE_WEB_VER_SUBVS ##. \
                ##APACHE_WEB_VER_BUILD)

#define APACHE_KEY_TOKEN "9b9b842f49b86351"

#ifdef AP_DECLARE_MODULE
AP_DECLARE_MODULE(mod_aspdotnet);
#endif

#endif // APACHE_WEB_VERSION
