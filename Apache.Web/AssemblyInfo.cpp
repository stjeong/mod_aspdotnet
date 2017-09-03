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

// AssemblyInfo.cpp support for Apache.Web Assembly

#include "Apache.Web.Version.h"

#using <mscorlib.dll>

using namespace System::Reflection;
using namespace System::Runtime::CompilerServices;

[assembly:AssemblyTitleAttribute("Apache.Web")];
[assembly:AssemblyDescriptionAttribute("Apache Web Hosting Architecture "
                                       "for Microsoft ASP.NET")];
[assembly:AssemblyCompanyAttribute("Covalent Technologies")];
[assembly:AssemblyProductAttribute("mod_aspdotnet")];
[assembly:AssemblyCopyrightAttribute("Copyright @2007 Covalent Technologies")];
[assembly:AssemblyTrademarkAttribute("")];
[assembly:AssemblyCultureAttribute("")];
[assembly:AssemblyConfigurationAttribute("")];
[assembly:AssemblyVersionAttribute(APACHE_WEB_VERSION)];

// Package is compiled, linked, then the manifest is embedded.
// It must be signed afterwards.
[assembly:AssemblyDelaySignAttribute(true)];
[assembly:AssemblyKeyFileAttribute("Apache.Web.snk")];
[assembly:AssemblyKeyNameAttribute("")];

