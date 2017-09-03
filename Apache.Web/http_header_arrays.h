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

// String constants for Apache::Web::Request.h that correspond to 
// the System.Web.HttpWorkerRequest KnownRequest/ResponseHeader values.
// These are macroized so that we are assured of building both bstr and
// char* flavors without any discrepancies between the two flavors.
// 
#pragma once

#define common_headers_name_m(t) \
    t##"Cache-Control",        \
    t##"Connection",           \
    t##"Date",                 \
    t##"Keep-Alive",           \
    t##"Pragma",               \
    t##"Trailer",              \
    t##"Transfer-Encoding",    \
    t##"Upgrade",              \
    t##"Via",                  \
    t##"Warning",              \
    t##"Allow",                \
    t##"Content-Length",       \
    t##"Content-Type",         \
    t##"Content-Encoding",     \
    t##"Content-Language",     \
    t##"Content-Location",     \
    t##"Content-MD5",          \
    t##"Content-Range",        \
    t##"Expires",              \
    t##"Last-Modified"

#define response_headers_m(t)  \
    common_headers_name_m(t),  \
    t##"Accept-Ranges",        \
    t##"Age",                  \
    t##"ETag",                 \
    t##"Location",             \
    t##"Proxy-Authenticate",   \
    t##"Retry-After",          \
    t##"Server",               \
    t##"Set-Cookie",           \
    t##"Vary",                 \
    t##"WWW-Authenticate"

#define request_headers_m(t)   \
    common_headers_name_m(t),  \
    t##"Accept",               \
    t##"Accept-Charset",       \
    t##"Accept-Encoding",      \
    t##"Accept-Language",      \
    t##"Authorization",        \
    t##"Cookie",               \
    t##"Expect",               \
    t##"From",                 \
    t##"Host",                 \
    t##"If-Match",             \
    t##"If-Modified-Since",    \
    t##"If-None-Match",        \
    t##"If-Range",             \
    t##"If-Unmodified-Since",  \
    t##"Max-Forwards",         \
    t##"Proxy-Authorization",  \
    t##"Referer",              \
    t##"Range",                \
    t##"TE",                   \
    t##"User-Agent"

#define common_headers_index_m(t) \
    t##"cache-control",        \
    t##"connection",           \
    t##"date",                 \
    t##"keep-alive",           \
    t##"pragma",               \
    t##"trailer",              \
    t##"transfer-encoding",    \
    t##"upgrade",              \
    t##"via",                  \
    t##"warning",              \
    t##"allow",                \
    t##"content-length",       \
    t##"content-type",         \
    t##"content-encoding",     \
    t##"content-language",     \
    t##"content-location",     \
    t##"content-md5",          \
    t##"content-range",        \
    t##"expires",              \
    t##"last-modified"

#define response_headers_index_m(t) \
    common_headers_index_m(t), \
    t##"accept-ranges",        \
    t##"age",                  \
    t##"etag",                 \
    t##"location",             \
    t##"proxy-authenticate",   \
    t##"retry-after",          \
    t##"server",               \
    t##"set-cookie",           \
    t##"vary",                 \
    t##"www-authenticate"

#define request_headers_index_m(t) \
    common_headers_index_m(t), \
    t##"accept",               \
    t##"accept-charset",       \
    t##"accept-encoding",      \
    t##"accept-language",      \
    t##"authorization",        \
    t##"cookie",               \
    t##"expect",               \
    t##"from",                 \
    t##"host",                 \
    t##"if-match",             \
    t##"if-modified-since",    \
    t##"if-none-match",        \
    t##"if-range",             \
    t##"if-unmodified-since",  \
    t##"max-forwards",         \
    t##"proxy-authorization",  \
    t##"referer",              \
    t##"range",                \
    t##"te",                   \
    t##"user-agent"
