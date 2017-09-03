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

// Apache::Web::HostFactory.h managed host engine for mod_aspdotnet
// initialization


#pragma once

#ifndef APACHE_WEB_H
#error "#include \"Apache.Web.h\" ... not HostFactory.h!"
#endif

namespace Apache
{
  namespace Web
  {
    // Forward declare a Host
    ref class Host;

    // The HostFactory engine to instantiate new Host objects
    public ref class HostFactory 
        : public IApacheWebHostFactory, public MarshalByRefObject
    {
    private:
        // Create our utf8-encoding to handle utf8->unicode names
        Encoding ^utf8_encoding;
        ArrayList ^hostURI;
        ArrayList ^hostDir;
        ArrayList ^hostObj;
        ArrayList ^hostSvr;
        int this_module_index;

    private:
        // Reports an error message in the error log
        void LogServerError(String ^msg, int loglevel, int rv, server_rec *s)
        {
            array<unsigned char> ^avalue = utf8_encoding->GetBytes(msg);
            pin_ptr<unsigned char> msg_text = &avalue[0];
            ap_log_error("Apache.Web", 0, this->this_module_index, loglevel|(rv ? 0 : APLOG_NOERRNO),
                         rv, s, "mod_aspdotnet: %s", (char*) msg_text, NULL);
        }


    public:
        HostFactory() {
            utf8_encoding = gcnew UTF8Encoding(false);
            hostURI = gcnew ArrayList();
            hostDir = gcnew ArrayList();
            hostObj = gcnew ArrayList();
            hostSvr = gcnew ArrayList();
        }

        virtual Object ^ InitializeLifetimeService() override
        {
            ILease ^lease = safe_cast<ILease^>
                             (MarshalByRefObject::InitializeLifetimeService());
            if (lease->CurrentState
                 == System::Runtime::Remoting::Lifetime::LeaseState::Initial) {
                lease->InitialLeaseTime = TimeSpan::FromDays(366);
                lease->SponsorshipTimeout = TimeSpan::FromMinutes(2);
                lease->RenewOnCallTime = TimeSpan::FromMinutes(1);
            }
            return lease;
        }

        // Returns E_FAIL if HostFactory cannot be configured;
        // path is a noop argument
        virtual HRESULT Configure(int module_index, String ^path) 
        {
            this->this_module_index = module_index;

#ifdef _DEBUG
            if (!WorkerRequest::CodeValidate())
                return E_FAIL;
            else
#endif
                return S_OK;
        }

        virtual void Destroy(void)
        {
            if (utf8_encoding) {
                utf8_encoding = nullptr;
                int hosts = hostObj->Count;
                for (int i = 0; i < hosts; ++i)
                {
                    // Don't fault if the hostObj is already destroyed
                    try {
                        hostObj[i] = nullptr;
                    }
                    catch (Exception ^ /*e*/) {
                    }
                }
                hostURI = nullptr;
                hostDir = nullptr;
                hostObj = nullptr;
                hostSvr = nullptr;
            }
        }

        // Instantiate a new Host object; trailing slashes are acceptable
        bool ConnectHost(int HostKey) 
        {
            Type ^hostType = Host::typeid;
            Object ^newHost;

            try {
                newHost = ApplicationHost::CreateApplicationHost(hostType,
                             safe_cast<String^>(hostURI[HostKey]),
                             safe_cast<String^>(hostDir[HostKey]));
                hostObj[HostKey] = newHost;
                                  
                if (newHost) {
                    Host ^host = dynamic_cast<Host^>(newHost);
                    host->Configure(
                             safe_cast<String^>(hostURI[HostKey]),
                             safe_cast<String^>(hostDir[HostKey]),
                             this, HostKey, this_module_index);
                    return true;
                }
                else {
                    server_rec *s = 
                        (server_rec *)(static_cast<UINT_PTR>(hostSvr[HostKey]));
                    LogServerError(L"mod_aspdotnet: CreateApplicationHost "
                                   L"returned NULL, perhaps the AspNetMount "
                                   L"paths were invalid", APLOG_ERR, 0, s);
                }
            }
            catch (Exception ^e) {
                server_rec *s = (server_rec *)
                        (server_rec *)(static_cast<UINT_PTR>(hostSvr[HostKey]));
                LogServerError(L"mod_aspdotnet: CreateApplicationHost failed, "
                               L"Exception follows", APLOG_ERR, 0, s);
                LogServerError(e->ToString(), APLOG_ERR, 0, s);
                hostObj[HostKey] = nullptr;
            }
            return false;
        }

        void HostFinalized(int HostKey, Host ^thisHost)
        {
            // Destroy only the correct host
            Threading::Monitor::Enter(this);
            if (thisHost->Equals(hostObj[HostKey]))
            {
                try {
                    hostObj[HostKey] = nullptr;
                }
                catch (Exception ^ /*e*/) {
                }
            }
            Threading::Monitor::Exit(this);
        }

        // Instantiate a new Host object; trailing slashes are acceptable
        virtual int CreateHost(String ^virtualPath, String ^physicalPath,
                               UINT_PTR ServerRecPtr) 
        {
            int newHostKey;

            physicalPath = physicalPath->Replace(L'/', L'\\');

            newHostKey = hostObj->Count;
            hostSvr->Add(ServerRecPtr);
            hostURI->Add(virtualPath);
            hostDir->Add(physicalPath);
            hostObj->Add(nullptr);

            if (!ConnectHost(newHostKey)) {
                server_rec *s = (server_rec *)ServerRecPtr;
                String ^msg = String::Concat(L"Mount failure creating host ",
                          safe_cast<String^>(hostURI[newHostKey]),
                                             L" mapped to ",
                          safe_cast<String^>(hostDir[newHostKey]));                                                
                LogServerError(msg, APLOG_ERR, 0, s);
                newHostKey = -1;
            }

            return newHostKey;
        }

        virtual int HandleHostRequest(int HostKey, UINT_PTR ReqRecordPtr) {
            request_rec *rr = (request_rec *)ReqRecordPtr;
            String ^msg;

            Object ^obj = hostObj[HostKey];
            if (obj != nullptr) {
                try {
                    Host ^host = safe_cast<Host^>(obj);
                    int status = host->HandleRequest(ReqRecordPtr);
                    return status;
                }
                // Presume all Remoting/AppDomainUnloadedExceptions 
                // require us to recreate the Host.  Fall through...
                catch (System::Runtime::Remoting::RemotingException ^e) {
                    msg = String::Concat(L"RemotingException; ",
                                         e->ToString());
                    LogServerError(msg, APLOG_DEBUG, 0, rr->server);
                }
                catch (System::AppDomainUnloadedException ^e) {
                    msg = String::Concat(L"AppDomainUnloadedException; ",
                                         e->ToString());
                    LogServerError(msg, APLOG_DEBUG, 0, rr->server);
                }
                catch (Exception ^e) {
                    msg = String::Concat(L"HandleRequest: Failed with "
                                         L"unexpected Exception; ",
                                         e->ToString());                                                
                    LogServerError(msg, APLOG_ERR, 0, rr->server);
                    return 500;
                }
            }

            msg = String::Concat(L"Restarting host of ",
                       safe_cast<String^>(hostURI[HostKey]),
                                 L" mapped to ",
                       safe_cast<String^>(hostDir[HostKey]));
            LogServerError(msg, APLOG_INFO, 0, rr->server);

            int welocked = 0;
            try {
                Threading::Monitor::Enter(this);
                welocked = 1;
                Object ^tryobj = hostObj[HostKey];
                if (tryobj == obj)
                {
                    // Release all stale obj references and catch any
                    // sort of dispatch exceptions (and ignore them).
                    try {
                        hostObj[HostKey] = nullptr;
                    }
                    catch (Exception ^ /*e*/) {
                    }
                    try {
                        obj = tryobj;
                    }
                    catch (Exception ^ /*e*/) {
                    }
                }
                if (obj == nullptr) {
                    if (!ConnectHost(HostKey)) {
                        Threading::Monitor::Exit(this);
                        msg = String::Concat(L"Mount failed to connect ",
                                             L"to the restarting host ",
                                   safe_cast<String^>(hostURI[HostKey]),
                                             L" mapped to ",
                                   safe_cast<String^>(hostDir[HostKey]));                                                
                        LogServerError(msg, APLOG_ERR, 0, rr->server);
                        return 500;
                    }
                    obj = hostObj[HostKey];
                    if (!obj) {
                        Threading::Monitor::Exit(this);
                        msg = String::Concat(L"Mount failed to query ",
                                             L"the restarting host ",
                                   safe_cast<String^>(hostURI[HostKey]),
                                             L" mapped to ",
                                   safe_cast<String^>(hostDir[HostKey]));                                                
                        LogServerError(msg, APLOG_ERR, 0, rr->server);
                        return 500;
                    }
                }
                Threading::Monitor::Exit(this);
            }
            catch (Exception ^e) {
                if (welocked) {
                    Threading::Monitor::Exit(this);
                }
                msg = String::Concat(L"Mount failure restarting host ",
                           safe_cast<String^>(hostURI[HostKey]),
                                     L" mapped to ",
                           safe_cast<String^>(hostDir[HostKey]));
                LogServerError(msg, APLOG_ERR, 0, rr->server);
                msg = String::Concat(L"Mount failure exception restarting "
                                     L"host; ", e->ToString());
                LogServerError(msg, APLOG_ERR, 0, rr->server);
                return 500;
            }

            try {
                Host ^host = safe_cast<Host^>(obj);
                int status = host->HandleRequest(ReqRecordPtr);
                return status;
            }
            catch (Exception ^e) {
                msg = String::Concat(L"RequestHandler; failure following "
                                     L"host restart; ", e->ToString());                                                
                LogServerError(msg, APLOG_ERR, 0, rr->server);
            }
            return 500;
        }

    protected:
        ~HostFactory()
        {
            Destroy();
        }
    };
  }
}
