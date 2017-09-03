/* Copyright 2002-2006 Covalent Technologies
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

// Apache::Web::WorkerRequest.cpp support code for mod_aspdotnet


#using <mscorlib.dll>
#using <System.Web.dll>
#include "Apache.Web.h"

Apache::Web::Host::~Host()
{
    // thisFactory may have already evaporated, ignore the fault
    try {
        thisFactory->HostFinalized(thisHostKey, this);
    }
    catch (Exception ^ /*e*/) {
    }
}

Apache::Web::WorkerRequest::WorkerRequest(UINT_PTR Req, Host ^ThisHost) 
        : SimpleWorkerRequest(String::Empty, String::Empty, nullptr)
{
    // Initialize empty placeholders
    request_headers_value = gcnew array<String^>(RequestHeaderMaximum);
    unk_req_header_name = gcnew ArrayList();
    unk_req_header_index = gcnew ArrayList();
    unk_req_header_value = gcnew ArrayList();
    env_name = gcnew ArrayList();
    env_value = gcnew ArrayList();
    serverName = nullptr;
    remoteName = nullptr;
    seen_eos = 0;
    body_sent = 0;
    bytes_read = 0;

    // Initialize state pointers to our encoding, request and host
    rr = (request_rec *)Req;
    host = ThisHost;

    // Deal with the usual request variables
    uri = gcnew String(rr->uri);
    unparsed_uri = gcnew String(rr->unparsed_uri);

    queryargs = gcnew String(rr->args);
    if (!rr->args || !*rr->args) {
        query_raw = gcnew array<unsigned char>(0);
    }
    else {
        apr_size_t len = strlen(rr->args);
        query_raw = gcnew array<unsigned char>(len);
        pin_ptr<unsigned char> foo = &query_raw[0];
        memcpy(foo, rr->args, len);
    }

    // rr->filename is utf-8 encoded for Apache 2.0/WinNT
    filename = gcnew String(rr->filename ? rr->filename : "", 0, 
                            rr->filename ? strlen(rr->filename) : 0, 
                            utf8_encoding);
    filename = filename->Replace(L'/', L'\\');

    // Now deal with the client's request headers
    const apr_array_header_t *arr = apr_table_elts(rr->headers_in);
    const char *elt = arr->elts;
    for (int i = 0; i < arr->nelts; ++i, elt += arr->elt_size) {
        apr_table_entry_t *pair = (apr_table_entry_t *)(void*)(elt);
        String ^key = gcnew String(pair->key);
        String ^val = gcnew String(pair->val);
        String ^key_index = gcnew String(key->ToLower(nullCulture));
        int ent = Array::IndexOf(request_headers_index_s, key_index);
        if (ent >= 0) {
            request_headers_value[ent] = val;
        }
        else {
            unk_req_header_name->Add(key);
            unk_req_header_index->Add(key_index);
            unk_req_header_value->Add(val);
        }
    }

    int elts = unk_req_header_value->Count;
    unk_req_hdr_arr = gcnew array<array<String^>^>(elts);

    for (int i = 0; i < elts; ++i) {
        array<String^> ^ent = gcnew array<String^>(2);
        ent[0] = static_cast<String ^>(unk_req_header_name[i]);
        ent[1] = static_cast<String ^>(unk_req_header_value[i]);
        unk_req_hdr_arr[i] = ent;
    }

    arr = apr_table_elts(rr->subprocess_env);
    elt = arr->elts;
    for (int i = 0; i < arr->nelts; ++i, elt += arr->elt_size) {
        apr_table_entry_t *pair = (apr_table_entry_t *)(void*)(elt);
        String ^var = gcnew String(pair->key);
        String ^val = gcnew String(pair->val);
        env_name->Add(var);
        env_value->Add(val);
    }
}

// Functions that require the full declaration 
// of Apache::Web::::Host which must be declared 
// after Apache::Web::WorkerRequest
//

void Apache::Web::WorkerRequest::LogRequestError(String ^msg, int loglevel, 
                                                 int rv) {
    host->LogRequestError(msg, loglevel, rv, rr);
}

String ^ Apache::Web::WorkerRequest::MapPath(String ^path)
{
    String ^mapped = host->MapPath(path, rr);
#ifdef _DEBUG
    String ^res = String::Concat(L"MapPath: ", path, 
                                 L" mapped to ", mapped);
    LogRequestError(res, APLOG_DEBUG, 0);
#endif
    return mapped;
}

String ^ Apache::Web::WorkerRequest::GetAppPath(void)
{
#ifdef _DEBUG
    String ^res = String::Concat(L"GetAppPath: returns ", 
                                 host->GetVirtualPath());
    LogRequestError(res, APLOG_DEBUG, 0);
#endif
    return host->GetVirtualPath();
}

String ^ Apache::Web::WorkerRequest::GetAppPathTranslated(void) 
{
    String ^bslashed = host->GetPhysicalPath();
    bslashed = bslashed->Insert(bslashed->Length, L"\\");
#ifdef _DEBUG
    String ^res = String::Concat(L"GetAppPathTranslated: returns ", bslashed);
    LogRequestError(res, APLOG_DEBUG, 0);
#endif
    return bslashed;
}

String ^ Apache::Web::WorkerRequest::GetFilePath(void)
{
    // Break URI matching filename, ignoring path_info
    String ^strip = host->GetPhysicalPath();
    String ^path = filename;
    if (path->StartsWith(strip)) {
        path = path->Substring(strip->Length);
        path = path->Insert(0, host->GetVirtualPath());
        path = path->Replace(L'\\', L'/');
    }
#ifdef _DEBUG
    String ^res = String::Concat(L"GetFilePath: returns ", path);
    LogRequestError(res, APLOG_DEBUG, 0);
#endif
    return path;
}


#pragma unmanaged
static apr_status_t InternalFlushResponse(request_rec *rr)
{   
    conn_rec *c = rr->connection;
    apr_bucket_brigade *bb;
    apr_bucket *b;

    bb = apr_brigade_create(rr->pool, c->bucket_alloc);
    b = apr_bucket_eos_create(c->bucket_alloc);
    APR_BRIGADE_INSERT_TAIL(bb, b);
    return ap_pass_brigade(rr->output_filters, bb);
}
#pragma managed

void Apache::Web::WorkerRequest::FlushResponse(bool finalFlush)
{
    if (finalFlush) {
        InternalFlushResponse(rr);

        // Even a a no-body request, the body was flushed
        body_sent = 1;

        // Report completion
        if (eosnCallback) {
            eosnCallback->Invoke(this, eosnExtra);
            eosnCallback = nullptr;
        }
#ifdef _DEBUG
        LogRequestError(L"FlushResponse: final", APLOG_DEBUG, 0);
#endif
    }
#ifdef _DEBUG
    else {
        LogRequestError(L"FlushResponse: incomplete", APLOG_DEBUG, 0);
    }
#endif
}


#pragma unmanaged
static apr_status_t InternalResponseFromMemory(request_rec *rr,
                                               const char *buf_data,
                                               int length)
{   
    conn_rec *c = rr->connection;
    apr_bucket_brigade *bb;
    apr_bucket *b;

    bb = apr_brigade_create(rr->pool, c->bucket_alloc);
    b = apr_bucket_transient_create(buf_data, length, c->bucket_alloc);
    APR_BRIGADE_INSERT_TAIL(bb, b);
    b = apr_bucket_flush_create(c->bucket_alloc);
    APR_BRIGADE_INSERT_TAIL(bb, b);
    return ap_pass_brigade(rr->output_filters, bb);
}
#pragma managed

void Apache::Web::WorkerRequest::SendResponseFromMemory(IntPtr data, 
                                                        int length)
{   
    if (!length) {
#ifdef _DEBUG
        LogRequestError("SendResponseFromMemory: passed zero byte buffer!", 
                        APLOG_DEBUG, 0);
#endif
        return;
    }
    const char *buf_data = static_cast<const char *>(data.ToPointer());

#ifdef _DEBUG
    String ^res = String::Concat(L"SendResponseFromMemory: passed ", 
                                 (new Int32(length))->ToString(), 
                                 L" byte buffer"); 
    LogRequestError(res, APLOG_DEBUG, 0);
#endif
    apr_status_t rv = InternalResponseFromMemory(rr, buf_data, length);
    body_sent = 1;

    // Failure, terminate request processing 
    if (rv != APR_SUCCESS)  {
        LogRequestError(L"SendResponseFromMemory: failure", APLOG_WARNING, rv);
        //   && eosnCallback) {
        // eosnCallback->Invoke(this, eosnExtra);
    }
}

#pragma unmanaged
static apr_status_t InternalResponseFromFile(request_rec *rr, HANDLE hv,
                                             __int64 offset, __int64 length) 
{
    conn_rec *c = rr->connection;
    apr_bucket_brigade *bb;
    apr_bucket *b;
    apr_file_t *fd;
    apr_off_t fsize;
    apr_status_t rv;

    rv = apr_os_file_put(&fd, &hv, APR_READ | APR_XTHREAD, rr->pool);
    if (rv != APR_SUCCESS) {
        return rv;
    }
    if (length) {
        fsize = length;
    }
    else {
        apr_finfo_t fi;
        rv = apr_file_info_get(&fi, APR_FINFO_SIZE, fd);
        if (rv != APR_SUCCESS) {
            return rv;
        }
        fsize = fi.size - offset;
    }

    bb = apr_brigade_create(rr->pool, c->bucket_alloc);
#if APR_HAS_LARGE_FILES
    if (rr->finfo.size > AP_MAX_SENDFILE) {
        // APR_HAS_LARGE_FILES issue; must split into mutiple buckets,
        // no greater than MAX(apr_size_t) .. we trust AP_MAX_SENDFILE
        //
        b = apr_bucket_file_create(fd, offset, AP_MAX_SENDFILE, 
                                   rr->pool, c->bucket_alloc);
        while (fsize > AP_MAX_SENDFILE) {
            apr_bucket *bc;
            apr_bucket_copy(b, &bc);
            APR_BRIGADE_INSERT_TAIL(bb, bc);
            b->start += AP_MAX_SENDFILE;
            fsize -= AP_MAX_SENDFILE;
        }
        b->length = (apr_size_t)fsize;  // Resize just the last bucket
    }
    else
#endif
        b = apr_bucket_file_create(fd, offset, (apr_size_t)fsize, 
                                    rr->pool, c->bucket_alloc);
    APR_BRIGADE_INSERT_TAIL(bb, b);
    b = apr_bucket_flush_create(c->bucket_alloc);
    APR_BRIGADE_INSERT_TAIL(bb, b);
    return ap_pass_brigade(rr->output_filters, bb);
}
#pragma managed

void Apache::Web::WorkerRequest::SendResponseFromFile(IntPtr handle, 
                                                      __int64 offset, 
                                                      __int64 length)
{
    if (!length) {
#ifdef _DEBUG
        LogRequestError("SendResponseFromFile: passed zero byte length!", 
                        APLOG_DEBUG, 0);
#endif
        return;
    }

#ifdef _DEBUG
    String ^res = String::Concat(L"SendResponseFromFile: passed ", 
                                 (new Int64(length))->ToString(), 
                                 L" byte request starting at offset ",
                                 (new Int64(offset))->ToString()); 
    LogRequestError(res, APLOG_DEBUG, 0);
#endif

    HANDLE hv = static_cast<HANDLE>(handle.ToPointer());

    apr_status_t rv = InternalResponseFromFile(rr, hv, offset, length);
    body_sent = 1;

    // Failure, terminate request processing 
    if (rv != APR_SUCCESS) {
        LogRequestError(L"SendResponseFromFile: failure", APLOG_WARNING, rv);
        //   && eosnCallback) {
        // eosnCallback->Invoke(this, eosnExtra);
    }
}


#pragma unmanaged
static apr_status_t InternalReadEntityBody(request_rec *rr, char *curptr,
                                           apr_size_t size, int *bytes_read,
                                           int *overflow, int *seen_eos)
{
    apr_bucket_brigade *bb;
    apr_status_t rv;
    *overflow = 0;
    *bytes_read = 0;

    // Reader-Writer helpers
    apr_bucket *dummy_flush_bucket;
    apr_bucket *dummy_eos_bucket;
    dummy_flush_bucket = apr_bucket_flush_create(rr->connection->bucket_alloc);
    dummy_eos_bucket = apr_bucket_eos_create(rr->connection->bucket_alloc);

    bb = apr_brigade_create(rr->pool, rr->connection->bucket_alloc);
    do {
        apr_bucket *bucket;
        apr_size_t size_one = (HUGE_STRING_LEN <= size? HUGE_STRING_LEN: size);

        if ((rv = ap_get_brigade(rr->input_filters, bb, 
                                 AP_MODE_READBYTES,
                                 APR_BLOCK_READ, size_one))
                != APR_SUCCESS) {
            return rv;
        }

        while (!APR_BRIGADE_EMPTY(bb)) {
            bucket = APR_BRIGADE_FIRST(bb);
            if (bucket->type == dummy_eos_bucket->type) {
                *seen_eos = 1;
                apr_bucket_delete(bucket);
                break;
            }
            if (bucket->type == dummy_flush_bucket->type) {
                apr_bucket_delete(bucket);
                continue;
            }
            const char *data;
            apr_size_t len;
            rv = apr_bucket_read(bucket, &data, &len, APR_BLOCK_READ);
            if (rv != APR_SUCCESS) {
                return rv;
            }
            if (len > size) {
                *overflow += len - size;
                len = size;
            }
            if (size) {
                memcpy(curptr, data, len);
                *bytes_read += len;
                curptr += len;
                size -= len;
            }
            // if (*overflow) above, might prefer to split this bucket, 
            // but we were dumping the bb anyways
            apr_bucket_delete(bucket);
        }
        apr_brigade_cleanup(bb);
    }
    while (!*seen_eos && (size > 0));

    return APR_SUCCESS;
}
#pragma managed


int Apache::Web::WorkerRequest::ReadEntityBody(array<unsigned char> ^buffer,
                                               int size) 
{
    if (buffer->Length < 1) {
#ifdef _DEBUG
        LogRequestError(L"ReadEntityBody: noread (passed 0 byte buffer)",
                        APLOG_DEBUG, 0);
#endif
        return 0;
    }
    pin_ptr<int> eos = &seen_eos;
    int read;
    int overflow;
    pin_ptr<unsigned char> lockbuff = &buffer[0];
    void *startptr = static_cast<void *>(lockbuff);
    apr_status_t rv = InternalReadEntityBody(rr, static_cast<char *>(startptr), 
                                             (apr_size_t)size, &read, &overflow,
                                             eos);
    bytes_read += read;
    if (rv != APR_SUCCESS) {
        LogRequestError(L"ReadEntityBody: brigade read failure "
                        L"reading the request body", APLOG_ERR, rv);
    }
    if (overflow) {
        LogRequestError(L"ReadEntityBody: brigade read overflow "
                        L"reading the request body", APLOG_ERR, rv);
    }
#ifdef _DEBUG
    String ^res = String::Concat(L"ReadEntityBody: read ", 
                                 (new Int32(read))->ToString(), 
                                 L" bytes");
    LogRequestError(res, APLOG_DEBUG, 0);
#endif
    return read;
}


int Apache::Web::WorkerRequest::Handler(void)
{
    wait_complete = gcnew System::Threading::ManualResetEvent(false);
    apr_bucket_brigade *bb;
    apr_status_t rv;
    apr_size_t size = 0;

    // Reader-Writer helpers
    //
    // Yes, this code is remarkably similar to InternalReadEntityBody,
    // however, for speed that code is entirely managed and incapable
    // of allocating a char ref [] that we need below.  In the code
    // below, we allocate exactly the correct ref [] required to
    // store the initial bytes, up to the MS semi-standard 49152.
    //
    apr_bucket *dummy_flush_bucket;
    apr_bucket *dummy_eos_bucket;
    dummy_flush_bucket = apr_bucket_flush_create(rr->connection->bucket_alloc);
    dummy_eos_bucket = apr_bucket_eos_create(rr->connection->bucket_alloc);

    bb = apr_brigade_create(rr->pool, rr->connection->bucket_alloc);

    apr_bucket *bucket;

    if ((rv = ap_get_brigade(rr->input_filters, bb, 
                             AP_MODE_READBYTES,
                             APR_BLOCK_READ, 49152))
            == APR_SUCCESS) {
        apr_off_t sz;
        rv = apr_brigade_length(bb, 1, &sz);
        size = (apr_size_t)sz;
        preread_body = gcnew array<unsigned char>(size);
        pin_ptr<unsigned char> lockbuff;
        char *curptr = NULL;
        if (size > 0) {
            lockbuff = &preread_body[0];
            void *startptr = static_cast<void *>(lockbuff);
            curptr = static_cast<char *>(startptr);
        }
        while (!APR_BRIGADE_EMPTY(bb)) {
            bucket = APR_BRIGADE_FIRST(bb);
            if (bucket->type == dummy_eos_bucket->type) {
                seen_eos = 1;
                apr_bucket_delete(bucket);
                break;
            }
            if (bucket->type == dummy_flush_bucket->type) {
    	        apr_bucket_delete(bucket);
                continue;
            }
            const char *data;
            apr_size_t len;
            rv = apr_bucket_read(bucket, &data, &len, APR_BLOCK_READ);
            if (rv != APR_SUCCESS) {
                LogRequestError(L"Handler: brigade read failure prereading "
                                L"the request body", APLOG_ERR, rv);
            }
            if (len > size) {
                len = size;
                LogRequestError(L"Handler: brigade read overflow prereading "
                                L"the request body", APLOG_ERR, 0);
            }
            if (size) {
                memcpy(curptr, data, len);
                bytes_read += len;
                curptr += len;
                size -= len;
            }
            // if (len > size) above, we overflowed, might prefer to
            // split this bucket, but we were dumping the bb anyways
            apr_bucket_delete(bucket);
        }
        apr_brigade_cleanup(bb);
#ifdef _DEBUG
        String ^res = String::Concat(L"Handler: preread ", 
                                    (new Int64(bytes_read))->ToString(), 
                                    L" request body bytes");
        LogRequestError(res, APLOG_DEBUG, 0);
#endif
    }
    else {
        LogRequestError(L"Handler: failed to preread the request body",
                        APLOG_ERR, rv);
        return 500;
    }

    try {
        HttpRuntime::ProcessRequest(this);
    }
    catch (Exception ^e) {
        if (!body_sent) {
            rr->status = 500;
            rr->status_line = ap_get_status_line(rr->status);
        }
        String ^msg = String::Concat(L"ProcessRequest: fatal exception "
                                     L"processing request ", uri, 
                                     L" exception follows;");
        LogRequestError(msg, APLOG_ERR, 0);
        LogRequestError(e->ToString(), APLOG_ERR, 0);
    }

    // Stall... if we didn't complete the request, we must wait
    // for EndOfRequest before being destroyed.
    wait_complete->WaitOne();

#ifdef _DEBUG
    LogRequestError(L"Handler: request processing complete", APLOG_DEBUG, 0);
#endif

    if (!body_sent) {
        return rr->status;
    }
    else {
        return OK;
    }
}
