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

// Apache.Web.h base COM interop interfaces for Apache 2.0

#ifndef APACHE_WEB_H
#define APACHE_WEB_H

#include "Apache.Web.Version.h"

// We don't care about unused arguments, 
// or conditional expression constant evaluation
#pragma warning(disable: 4100 4127)

// Apache namespace 
//    Native namespace : pure ap/apr API and structures
//    Web namespace    : interface to Server.Web.Hosting engine
//       HostFactory [Implements IApacheWebHostFactory] 
//          CreateHost singleton
//       Host [Implements IApacheWebHost]       
//          HandleRequest instance creation
//       WorkerRequest [Implements IApacheWebWorkerRequest] 
//          A single .Web request object

namespace Apache {
#pragma unmanaged
    namespace Native {
#include "httpd.h"
#include "http_core.h"
#include "http_config.h"
#include "http_log.h"
#include "http_protocol.h"
#include "apr_strings.h"
#include "apr_tables.h"
    }
#pragma managed
}

using namespace Apache::Native;

using namespace System;
using namespace System::IO;
using namespace System::Text;
using namespace System::Web;
using namespace System::Web::Hosting;
using namespace System::Runtime::InteropServices;
using namespace System::Runtime::Remoting::Lifetime;
//using namespace System::Diagnostics;

namespace Apache
{
  namespace Web
  {
    // Forward declare a Host
    ref class Host;
    
    // Define using dispinterfaces for COM transition to ASP.Net
    public interface class IApacheWebHostFactory
    {
    public:
         HRESULT Configure(int module_index, String ^path);
         void Destroy(void);
         // Returns HostKey to pass on to HandleHostRequest()
         int CreateHost(String ^virtualPath, String ^physicalPath,
                        UINT_PTR ServerRecPtr);
         // Returns HTTP Status from ApacheWebHost::HandleRequest
         int HandleHostRequest(int HostKey, UINT_PTR ReqRecordPtr);
    };
  }
}

#include "WorkerRequest.h"
#include "Host.h"
#include "HostFactory.h"

// End of Apache::Web Namespace declarations

#endif // APACHE_WEB_H
