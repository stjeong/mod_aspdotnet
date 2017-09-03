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

// Apache::Web::Host.h managed interface for mod_aspdotnet connector namespaces


#pragma once

#ifndef APACHE_WEB_H
#error "#include \"Apache.Web.h\" ... not Host.h!"
#endif
#include <stdarg.h>

namespace Apache
{
  namespace Web
  {
    ref class HostFactory;

    public ref class Host : public MarshalByRefObject
    {
    private:
        // Create our utf8-encoding to handle utf8->unicode names
        static Encoding ^utf8_encoding = gcnew UTF8Encoding(false);

        // our default cultureNeutral object for proper strcmp's
        static CultureInfo ^nullCulture = CultureInfo::InvariantCulture;

        // No trailing slash for this URI-path
        String ^virtualPath;
        // No trailing backslash for this filesystem path
        String ^physicalPath;

        HostFactory ^thisFactory;
        int thisHostKey;
        int thisModuleIndex;

    protected:
        ~Host();

    public:
        Host()
        {
            thisFactory = nullptr;
            thisHostKey = 0;
        }

        virtual Object ^ InitializeLifetimeService() override
        {
            ILease ^lease = safe_cast<ILease ^>
                             (MarshalByRefObject::InitializeLifetimeService());
            if (lease->CurrentState 
                 == System::Runtime::Remoting::Lifetime::LeaseState::Initial) {
                lease->InitialLeaseTime = TimeSpan::FromDays(366);
                lease->SponsorshipTimeout = TimeSpan::FromMinutes(2);
                lease->RenewOnCallTime = TimeSpan::FromMinutes(1);
            }
            return lease;
        }

        // Internal accessor for new Host construction
        void Configure(String ^VirtualPath, String ^PhysicalPath,
                       HostFactory ^Factory, int HostKey, int moduleIndex) 
        {
            //
            // The ApplicationHost::CreateApplicationHost() constructor
            // doesn't provide us an opportunity to deal with additional
            // arguments to our Host object constructor; Configure() makes
            // up for that shortcoming after the Host is instantiated.
            //
            thisFactory = Factory;
            thisHostKey = HostKey;
            thisModuleIndex = moduleIndex;
            physicalPath = PhysicalPath->Replace(L'/', L'\\');
            virtualPath = VirtualPath->Replace(L'\\', L'/');
            if (physicalPath->EndsWith(L"\\")) {
                physicalPath
                    = physicalPath->Substring(0, physicalPath->Length - 1);
            }
            if (virtualPath->EndsWith(L"/")) {
                virtualPath
                    = virtualPath->Substring(0, virtualPath->Length - 1);
            }
        }

        // Reports an error message in the error log
        void LogRequestError(String ^msg, int loglevel, apr_status_t rv,
                             request_rec *rr)
        {
            array<unsigned char> ^avalue  
                = utf8_encoding->GetBytes(msg);
            pin_ptr<unsigned char> msg_text = &avalue[0];
            ap_log_rerror("Apache.Web", 0, thisModuleIndex, loglevel | (rv ? 0 : APLOG_NOERRNO),
                          rv, rr, "mod_aspdotnet: %s", (char*) msg_text, NULL);
        }

        // Instantiates a new Apache.Web.WorkerRequest object from the given 
        // request_rec*
        int HandleRequest(UINT_PTR ReqRecordPtr) {
            try {
                WorkerRequest ^request = gcnew WorkerRequest(ReqRecordPtr,
                                                             this);
                int status = request->Handler();
                return status;
            }
            catch (Exception ^e) {
                LogRequestError(L"Failed to create ASP.NET Request, "
                                L"Exception follows", APLOG_ERR, 0,
                                (request_rec *)ReqRecordPtr);
                LogRequestError(e->ToString(), APLOG_ERR, 0, 
                                (request_rec *)ReqRecordPtr);
                return 500;
            }
        }

        // Returns the Virtual URI-path of this host, with no trailing slash
        String ^ GetVirtualPath(void) {
            return virtualPath;
        }

        // Returns the Physical filesystem path of this host,
        // with no trailing slash
        String ^ GetPhysicalPath(void) {
            return physicalPath;
        }

        // Translate the given URI-path in this host's Virtual path to 
        // a filesystem path
        String ^ MapPath(String ^path, request_rec *rr) {
            //
            // ...relative to this host's Physical path.  The function 
            // returns NULL if the given URI-path is outside of the Virtual
            // path of this host. 
            //
            String ^buildpath;

            if (!path || (path->Length <= 0)) {
                return physicalPath;
            }
            else {
                buildpath = path->Replace(L'/', L'\\');
            }

            String ^testpath = path->Replace(L'\\', L'/');

            if (testpath[0] == L'/') {
                if (String::Compare(testpath, 0, virtualPath, 0, 
                                    virtualPath->Length, 
                                    true, nullCulture)) {
                    String ^res = String::Concat(L"MapPath: could not map ",
                                                 path, L" within ",
                                                 virtualPath);
                    LogRequestError(res, APLOG_DEBUG, 0, rr);
                    return nullptr;
                }
                else {
                    if (testpath->Length > virtualPath->Length) {
                        buildpath = 
                            String::Concat(physicalPath,
                                           buildpath->Substring(
                                               virtualPath->Length));
                    }
                    else {
                        buildpath = physicalPath;
                    }
                }
            }
            else {
                buildpath = String::Concat(physicalPath, L"\\",
                                           buildpath->Substring(
                                               virtualPath->Length));
            }
            return buildpath;
        }
    };
  }
}
