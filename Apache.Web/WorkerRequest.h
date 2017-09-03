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

// Apache::Web::WorkerRequest.h managed interface for Apache 2.0 request_rec


#pragma once

#ifndef APACHE_WEB_H
#error "#include \"Apache.Web.h\" ... not WorkerRequest.h!"
#endif

#include "http_header_arrays.h"

using namespace System::Collections;
using namespace System::Globalization;
using namespace cli;

namespace Apache
{
  namespace Web
  {
    // Forward declare a Host
    ref class Host;

    public ref class WorkerRequest : public SimpleWorkerRequest
    {
    private:
        static array<const char *> ^response_headers_name_c = {
            response_headers_m("")
        };
     //* Unused variant, but trivial to enable later
     // static array<const char *> ^request_headers_name_c = {
     //     request_headers_m("")
     // };
        static array<String^> ^response_headers_name_s = {
            response_headers_m(L)
        };
        static array<String^> ^request_headers_name_s = {
            request_headers_m(L)
        };
        static array<String^> ^response_headers_index_s = {
            response_headers_index_m(L)
        };
        static array<String^> ^request_headers_index_s = {
            request_headers_index_m(L)
        };

        // Create our utf8-encoding to handle utf8->unicode names
        static Encoding ^utf8_encoding = gcnew UTF8Encoding(false);

        // Create our invariant culture for handling ASCII case
        static CultureInfo ^nullCulture = CultureInfo::InvariantCulture;
    
        String ^uri;
        String ^unparsed_uri;
        String ^queryargs;
        String ^filename;
        String ^remoteName;
        String ^serverName;

        array<unsigned char> ^query_raw;

        array<String^> ^request_headers_value;
        ArrayList ^unk_req_header_name;
        ArrayList ^unk_req_header_index;
        ArrayList ^unk_req_header_value;
        ArrayList ^env_name;
        ArrayList ^env_value;

        array<array<String^>^> ^unk_req_hdr_arr;

        request_rec *rr;

        Host ^host;
        System::Threading::ManualResetEvent ^wait_complete;
        EndOfSendNotification ^eosnCallback;
        Object ^eosnExtra;

        array<unsigned char> ^preread_body;
        int seen_eos;
        int body_sent;
        __int64 bytes_read;

        // Reports an error message in the error log
        void LogRequestError(String ^msg, int loglevel, apr_status_t rv);

    public:
        static bool CodeValidate() {
            if (response_headers_name_s->Length != ResponseHeaderMaximum)
                return false;
            if (response_headers_index_s->Length != ResponseHeaderMaximum)
                return false;
            for (int i = 0; i < ResponseHeaderMaximum; ++i) {
                String ^s = HttpWorkerRequest::GetKnownResponseHeaderName(i);
                if (response_headers_name_s[i]->CompareTo(s) != 0)
                    return false;
                if (response_headers_index_s[i]->CompareTo(
                                                     s->ToLower(nullCulture))
                                                  != 0)
                    return false;
            }
            if (request_headers_name_s->Length != RequestHeaderMaximum)
                return false;
            if (request_headers_index_s->Length != RequestHeaderMaximum)
                return false;
            for (int i = 0; i < RequestHeaderMaximum; ++i) {
                String ^s = HttpWorkerRequest::GetKnownRequestHeaderName(i);
                if (request_headers_name_s[i]->CompareTo(s) != 0)
                    return false;
                if (request_headers_index_s[i]->CompareTo(
                                                    s->ToLower(nullCulture))
                                                 != 0)
                    return false;
            }
            return true;
        };

        // Cannot be declared inline, we don't have Host defined
        virtual String ^ GetAppPath(void) override ; 
        virtual String ^ GetAppPathTranslated(void) override; 

        WorkerRequest(UINT_PTR ReqRecordPtr, Host ^ThisHost);

        int Handler(void);
        [ComVisible(false)]
        virtual void SetEndOfSendNotification(
                         HttpWorkerRequest::EndOfSendNotification ^callback,
                         Object ^extraData) override
        {
#ifdef _DEBUG
            LogRequestError(L"SetEndOfSendNotification: initialized", 
                            APLOG_DEBUG, 0);
#endif
        }

        virtual void EndOfRequest(void) override
        {
            // If ProcessRequest() in Handler completed before the request
            // request processing completes, it blocks on wait_complete, 
            // we need to toggle the event that Handler() is blocked on
            // or would block on upon return from ProcessRequest().
            if (eosnCallback) {
#ifdef _DEBUG
                LogRequestError(L"EndOfRequest: invoked EndOfSendNotification",
                                APLOG_DEBUG, 0);
#endif
                eosnCallback->Invoke(this, eosnExtra);
                eosnCallback = nullptr;
            }
#ifdef _DEBUG
            else
                LogRequestError(L"EndOfRequest: processed", APLOG_DEBUG, 0);
#endif
            wait_complete->Set();
        }

        virtual String ^ GetUriPath(void) override
        {
#ifdef _DEBUG
            String ^res = String::Concat(L"GetUriPath: returns ", uri);
            LogRequestError(res, APLOG_DEBUG, 0);
#endif
            return uri;
        }

        virtual String ^ GetQueryString(void) override
        {
#ifdef _DEBUG
            String ^res = String::Concat(L"GetQueryString: returns ",
                                         queryargs);
            LogRequestError(res, APLOG_DEBUG, 0);
#endif
            return queryargs;
        }

        virtual array<unsigned char> ^ GetQueryStringRawBytes(void) override
        {
#ifdef _DEBUG
            String ^res = String::Concat(L"GetQueryStringRawBytes: returns ",
                                    (new Int32(query_raw->Length))->ToString(),
                                         L" raw characters");
            LogRequestError(res, APLOG_DEBUG, 0);
#endif
            return query_raw;
        }

        virtual String ^ GetRawUrl(void) override
        {
#ifdef _DEBUG
            String ^res = String::Concat(L"GetRawUrl: returns ", unparsed_uri);
            LogRequestError(res, APLOG_DEBUG, 0);
#endif
            return unparsed_uri;
        }

        virtual String ^ GetHttpVerbName(void) override
        {
            String ^verb = gcnew String(rr->method);
#ifdef _DEBUG
            String ^res = String::Concat(L"GetHttpVerbName: returns ", verb);
            LogRequestError(res, APLOG_DEBUG, 0);
#endif
            return verb;
        }

        virtual String ^ GetHttpVersion(void) override
        {
            String ^version = gcnew String(rr->protocol);
#ifdef _DEBUG
            String ^res = String::Concat(L"GetHttpVersion: returns ", version);
            LogRequestError(res, APLOG_DEBUG, 0);
#endif
            return version;
        }

        virtual String ^ GetProtocol(void) override
        {
#if MODULE_MAGIC_NUMBER_MAJOR >= 20050101
            // Fixes in httpd-2.1-dev
            const char *method = ap_http_scheme(rr);
#else
            const char *method = ap_http_method(rr);
#endif
            String ^scheme = gcnew String(method);

            // For some silly reason, the ASP.NET team thought that schemes
            // are upper case entities;
            scheme = scheme->ToUpper(nullCulture);
#ifdef _DEBUG
            String ^res = String::Concat(L"GetProtocol: returns ", scheme);
            LogRequestError(res, APLOG_DEBUG, 0);
#endif
            return scheme;
        }

        virtual String ^ GetRemoteAddress(void) override
        {
            String ^addr = gcnew String(rr->connection->client_ip);
#ifdef _DEBUG
            String ^res = String::Concat(L"GetRemoteAddress: returns ", addr);
            LogRequestError(res, APLOG_DEBUG, 0);
#endif
            return addr;
        }

        virtual int GetRemotePort(void) override
        {
#ifdef _DEBUG
            String ^res = String::Concat(L"GetRemotePort: returns ", 
                (new Int32(rr->connection->client_addr->port))->ToString());
            LogRequestError(res, APLOG_DEBUG, 0);
#endif
	    return rr->connection->client_addr->port;
        }

        virtual String ^ GetRemoteName(void) override
        {
            if (!remoteName) {
                const char *name = ap_get_remote_host(rr->connection, 
                                                      rr->per_dir_config, 
                                                      REMOTE_HOST, NULL);
                if (!name)
                    remoteName = String::Empty;
                else
                    remoteName = gcnew String(name);
            }
#ifdef _DEBUG
            String ^res = String::Concat(L"GetRemoteName: returns ",
                                         remoteName);
            LogRequestError(res, APLOG_DEBUG, 0);
#endif
            return remoteName;
        }

        virtual String ^ GetServerName(void) override
        {
            if (!serverName) {
                const char *name = ap_get_server_name(rr);
                if (!name)
                    serverName = String::Empty;
                else
                    serverName = gcnew String(name);
            }
#ifdef _DEBUG
            String ^res = String::Concat(L"GetServerName: returns ",
                                         serverName);
            LogRequestError(res, APLOG_DEBUG, 0);
#endif
            return serverName;
        }

        virtual String ^ GetServerVariable(String ^name) override
        {
            if (name->Equals(L"ALL_RAW")) {
                int size = 1;
                for (int i = 0; i < env_name->Count; ++i) {
                    size += static_cast<String^>(env_name[i])->Length + 1;
                    size += static_cast<String^>(env_value[i])->Length + 1;
                }
                StringBuilder ^res = gcnew StringBuilder(size);
                for (int i = 0; i < env_name->Count; ++i) {
                    res = res->Append(static_cast<String ^>(env_name[i]));
                    res = res->Append(L"=");
                    res = res->Append(static_cast<String ^>(env_value[i]));
                    res = res->Append(L"\0", 0, 1);
                }
                res = res->Append(L"\0", 0, 1);
#ifdef _DEBUG
                LogRequestError(L"GetServerVariable: for ALL_RAW returns all",
                                APLOG_DEBUG, 0);
#endif
                return res->ToString();
            }
            int ent = env_name->IndexOf(name);
            if (ent >= 0) {
#ifdef _DEBUG
            String ^res = String::Concat(L"GetServerVariable: for ", name, 
                                         L" returns ", 
                                         static_cast<String^>(env_value[ent]));
            LogRequestError(res, APLOG_DEBUG, 0);
#endif
                return static_cast<String^>(env_value[ent]);
            }
#ifdef _DEBUG
            String ^res = String::Concat(L"GetServerVariable: for ", name,
                                         L" returns NULL (not found)");
            LogRequestError(res, APLOG_DEBUG, 0);
#endif
            return nullptr;
        }

        virtual String ^ GetLocalAddress(void) override
        {
            String ^addr = gcnew String(rr->connection->local_ip);
#ifdef _DEBUG
            String ^res = String::Concat(L"GetLocalAddress: returns ", addr);
            LogRequestError(res, APLOG_DEBUG, 0);
#endif
            return addr;
        }

        virtual int GetLocalPort(void) override
        {
#ifdef _DEBUG
            String ^res = String::Concat(L"GetLocalPort: returns ", 
                (new Int32(rr->connection->local_addr->port))->ToString());
            LogRequestError(res, APLOG_DEBUG, 0);
#endif
            return rr->connection->local_addr->port;
        }

        virtual String ^ GetFilePath(void) override;

        virtual String ^ GetFilePathTranslated(void) override
        {
#ifdef _DEBUG
            String ^res = String::Concat(L"GetFilePathTranslated: returns ",
                                         filename);
            LogRequestError(res, APLOG_DEBUG, 0);
#endif
            return filename;
        }

        virtual String ^ GetPathInfo(void) override
        {
            String ^path_info = gcnew String(rr->path_info);
#ifdef _DEBUG
            String ^res = String::Concat(L"GetPathInfo: returns ", path_info);
            LogRequestError(res, APLOG_DEBUG, 0);
#endif
            return path_info;
        }

        virtual String ^ GetKnownRequestHeader(int index) override
        {
            if (index >= 0 && index < RequestHeaderMaximum) {
#ifdef _DEBUG
                String ^res = String::Concat(L"GetKnownRequestHeader: for ", 
                                             request_headers_name_s[index],
                                             L" returns ", 
                                             request_headers_value[index]);
                LogRequestError(res, APLOG_DEBUG, 0);
#endif
                return request_headers_value[index];
            }
#ifdef _DEBUG
            String ^res = String::Concat(L"GetKnownRequestHeader: for ", 
                                         (new Int32(index))->ToString(),
                                         L" returns NULL (not found)");
            LogRequestError(res, APLOG_DEBUG, 0);
#endif
            return nullptr;
        }
    
        virtual String ^ GetUnknownRequestHeader(String ^name) override
        {
            int ent = unk_req_header_index->IndexOf(name->ToLower(nullCulture));
            if (ent >= 0) {
                String ^val = static_cast<String^>(unk_req_header_value[ent]);
#ifdef _DEBUG
                String ^res = String::Concat(L"GetUnknownRequestHeader: for ",
                                             name, L" returns ", val);
                LogRequestError(res, APLOG_DEBUG, 0);
#endif
                return val;
            }
#ifdef _DEBUG
            String ^res = String::Concat(L"GetUnknownRequestHeader: for ", name,
                                         L" returns NULL (not found)");
            LogRequestError(res, APLOG_DEBUG, 0);
#endif
            return nullptr;
        }

        [System::CLSCompliantAttribute(false)]
        virtual array<array<String^>^> ^ GetUnknownRequestHeaders(void) override
        {
#ifdef _DEBUG
            LogRequestError(L"GetUnknownRequestHeaders: returns all",
                            APLOG_DEBUG, 0);
#endif
            return unk_req_hdr_arr;
        }

        static int GetKnownRequestHeaderIndex(String ^header)
        {
            return Array::IndexOf(request_headers_index_s,
                                  header->ToLower(nullCulture));
        }

        static String ^ GetKnownRequestHeaderName(int header)
        {
            if (header >= 0 && header < RequestHeaderMaximum) {
                return request_headers_name_s[header];
            }
            return nullptr;
        }

        static int GetKnownResponseHeaderIndex(String ^header)
        {
            return Array::IndexOf(response_headers_index_s,
                                  header->ToLower(nullCulture));
        }

        static String ^ GetKnownResponseHeaderName(int header)
        {
            if (header >= 0 && header < ResponseHeaderMaximum) {
                return response_headers_name_s[header];
            }
            return nullptr;
        }

        //virtual bool HasEntityBody(void) override
        //{
#ifdef _DEBUG
        //    String ^res = String::Concat(L"HasEntityBody: returns ", 
        //        (new Boolean(!seen_eos || bytes_read))->ToString());
        //    LogRequestError(res, APLOG_DEBUG, 0);
#endif
        //    return !seen_eos || bytes_read;
        //}

        virtual int ReadEntityBody(array<unsigned char> ^buffer, int size)
                override;

        virtual __int64 GetBytesRead(void) override
        {
#ifdef _DEBUG
            String ^res = String::Concat(L"GetBytesRead: returns ", 
                                         (new Int64(bytes_read))->ToString());
            LogRequestError(res, APLOG_DEBUG, 0);
#endif            
            return bytes_read;
        }

        virtual array<unsigned char> ^GetPreloadedEntityBody(void) override
        {
#ifdef _DEBUG
            String ^res = String::Concat(L"GetPreloadedEntityBody: returns ", 
                (new Int32(preread_body->Length))->ToString(), 
                L" bytes");
            LogRequestError(res, APLOG_DEBUG, 0);
#endif
            return preread_body;
        }

        virtual bool IsEntireEntityBodyIsPreloaded(void) override
        {
#ifdef _DEBUG
            String ^res = String::Concat(L"IsEntireEntityBodyIsPreloaded:",
                                         L" returns ", 
                                         (new Boolean(seen_eos != 0))->ToString());
            LogRequestError(res, APLOG_DEBUG, 0);
#endif
            return seen_eos != 0;
        }

        virtual bool IsClientConnected(void) override
        {
#ifdef _DEBUG
            String ^res = String::Concat(L"IsClientConnected: returns ", 
                (new Boolean(!rr->connection->aborted))->ToString());
            LogRequestError(res, APLOG_DEBUG, 0);
#endif
            return !rr->connection->aborted;
        }

        virtual bool HeadersSent(void) override
        {
#ifdef _DEBUG
            String ^res = String::Concat(L"HeadersSent: returns ", 
                (new Boolean(body_sent != 0))->ToString());
            LogRequestError(res, APLOG_DEBUG, 0);
#endif
            return (body_sent != 0);
        }

        virtual void SendStatus(int statusCode, String ^statusDescription)
                override
        {
#ifdef _DEBUG
            String ^res = String::Concat(L"SendStatus: sending ", 
                                         (new Int32(statusCode))->ToString(), 
                                         L" with desc ", statusDescription);
            LogRequestError(res, APLOG_DEBUG, 0);
#endif
            rr->status = statusCode;
            if (!statusDescription 
                    || statusDescription->Equals(String::Empty)) {
                rr->status_line = ap_get_status_line(rr->status);
            }
            else {
                array<unsigned char> ^avalue 
                    = utf8_encoding->GetBytes(statusDescription);
                pin_ptr<unsigned char> status_desc = &avalue[0];
                char code[12];
                itoa(rr->status, code, 10);
                rr->status_line = apr_pstrcat(rr->pool, code, " ", 
                                              status_desc, NULL);
            }
        }
        
        virtual void SendKnownResponseHeader(int index, String ^value) override
        {
            if (index >= 0 && index < ResponseHeaderMaximum) {
#ifdef _DEBUG
                String ^res = String::Concat(L"SendKnownResponseHeader: ", 
                                             response_headers_name_s[index], 
                                             L" as ", value);
                LogRequestError(res, APLOG_DEBUG, 0);
#endif
                array<unsigned char> ^avalue = utf8_encoding->GetBytes(value);
                int len = avalue->Length;
                pin_ptr<unsigned char> cvalue = &avalue[0];
                char *poolval = static_cast<char *>(apr_pmemdup(rr->pool,
                                                                cvalue,
                                                                len + 1));
                poolval[len] = '\0';
                if (index == HeaderContentType)
                    ap_set_content_type(rr, poolval);
                else
                    apr_table_setn(rr->headers_out,
                                   response_headers_name_c[index], poolval);
            }
#ifdef _DEBUG
            else {
                String ^res = String::Concat(L"SendKnownResponseHeader: ",
                                             L"failed to send ", 
                                             (new Int32(index))->ToString(), 
                                             L" as ", value);
                LogRequestError(res, APLOG_DEBUG, 0);
            }
#endif
        }

        virtual void SendUnknownResponseHeader(String ^name, String ^value)
                override
        {
#ifdef _DEBUG
            String ^res = String::Concat(L"SendUnknownResponseHeader: ", name, 
                                         L" as ", value);
            LogRequestError(res, APLOG_DEBUG, 0);
#endif
            array<unsigned char> ^aname = utf8_encoding->GetBytes(name);
            int len = aname->Length;
            pin_ptr<unsigned char> cname = &aname[0];
            char *poolkey = static_cast<char*>(apr_pmemdup(rr->pool, cname,
                                                           len + 1));
            poolkey[len] = '\0';
            array<unsigned char> ^avalue = utf8_encoding->GetBytes(value);
            len = avalue->Length;
            pin_ptr<unsigned char> cvalue = &avalue[0];
            char *poolval = static_cast<char *>(apr_pmemdup(rr->pool, cvalue,
                                                            len + 1));
            poolval[len] = '\0';
            apr_table_setn(rr->headers_out, poolkey, poolval);
        }

        virtual void SendCalculatedContentLength(int contentLength) override
        {
#ifdef _DEBUG
            String ^res = String::Concat(L"SendCalculatedContentLength: ", 
                                        (new Int32(contentLength))->ToString());
            LogRequestError(res, APLOG_DEBUG, 0);
#endif
            ap_set_content_length(rr, contentLength);
        }

        virtual void SendResponseFromMemory(IntPtr data, int length) override;

        virtual void SendResponseFromMemory(array<unsigned char>^data,
                                            __int32 length) override
        {
            if (!length) {
#ifdef _DEBUG
                LogRequestError("SendResponseFromMemory: zero byte buffer!",
                                APLOG_DEBUG, 0);
#endif
                return;
            }
#ifdef _DEBUG
            LogRequestError(L"SendResponseFromFile: pinning GC buffer",
                            APLOG_DEBUG, 0);
#endif
            pin_ptr<unsigned char> data_buf = &data[0];
            IntPtr data_ptr = static_cast<IntPtr>(data_buf);
            SendResponseFromMemory(data_ptr, length);
        }

        virtual void SendResponseFromFile(IntPtr handle, __int64 offset,
                                          __int64 length) override;

        virtual void SendResponseFromFile(String ^_filename, __int64 offset,
                                          __int64 length) override
        {
            if (!length) {
#ifdef _DEBUG
                LogRequestError("SendResponseFromFile: zero byte buffer!",
                                APLOG_DEBUG, 0);
#endif
                return;
            }
#ifdef _DEBUG
            String ^res = String::Concat(L"SendResponseFromFile: opening ", 
                _filename);
            LogRequestError(res, APLOG_DEBUG, 0);
#endif
            FileStream ^stream = gcnew FileStream(_filename, FileMode::Open,
                                                  FileAccess::Read, 
                                                  FileShare::Read, 8192, true);
            SendResponseFromFile(stream->Handle, offset, length);
            stream->Close();
        }

        virtual void FlushResponse(bool finalFlush) override;

        virtual void CloseConnection(void) override
        {
#ifdef _DEBUG
            LogRequestError(L"CloseConnection: requested (noop)",
                            APLOG_DEBUG, 0);
#endif
            ; // Noop, we trust Apache's determination here of the protocol
        }

        virtual String ^ MapPath(String ^path) override;
    };
  }
}
