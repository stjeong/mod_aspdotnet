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

 // mod_aspdotnet.so - Microsoft ASP.NET connector module for Apache 2.0


 // We don't care about unused arguments
#pragma warning(disable: 4100)

// We require much Winfoo to intermix Com/Ole/CLR code...
// don't wait for APR to include a stripped down Win32 API
//
#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0400
#include <Windows.h>
#include <winsock2.h>
#include <objbase.h>
#include <ws2tcpip.h>

#include "apr.h"
#include "apr_lib.h"
#include "apr_strings.h"
#include "apr_buckets.h"
#include "apr_pools.h"

#include "httpd.h"
#include "http_core.h"
#include "http_config.h"
#include "http_log.h"
#include "http_main.h"
#include "http_request.h"
#include "util_filter.h"
#include "util_script.h"

#include "Apache.Web.Version.h"

#include <mscoree.h>
#import <mscorlib.tlb> raw_interfaces_only \
    high_property_prefixes("_get","_put","_putref") \
    rename("ReportEvent", "CorReportEvent")
using namespace mscorlib;

#include <MetaHost.h>

#define STRING2(x) #x
#define STRING(x) STRING2(x)

#import STRING(TLBDIR)

using namespace Apache_Web;

extern "C" {
    extern module AP_MODULE_DECLARE_DATA aspdotnet_module;
}

// Initialized or recovered (from userdata) in pre_config
typedef struct asp_net_host_conf_t
{
    // Allocation issue; virtual and physical are hashed
    // as a combined string of /vpath\0/ppath\0 with the
    // combined_len including both NULLs.  This permits us
    // to quickly find given virtual/physical mappings for
    // given vhosts (they are bound to a specific virutal
    // and physical path pair.)
    apr_size_t  combined_len;
    const char *virtual_path;
    const char *physical_path;
    int         host_key;
    //    IApacheWebHost *pHost;
} asp_net_host_conf_t;

struct asp_net_conf_t {
    apr_pool_t *pool;
    apr_hash_t *hosts;  // Hash of asp_net_host_conf_t records
    ICorRuntimeHost *pCorRuntime;
    IApacheWebHostFactory *pHostFactory;
    HMODULE lock_module;
    OSVERSIONINFO osver;
    char *cor_version;
} *global_conf;

// Initialized for each restart
typedef struct asp_net_alias_t {
    // These are hashed per-vhost only by the virtual_path.
    apr_size_t  virtual_path_len;
    const char *virtual_path;
    asp_net_host_conf_t *host;
} asp_net_alias_t;

typedef struct asp_net_srvconf_t {
    apr_hash_t *aliases;  // Hash of asp_net_alias_t records
} asp_net_srvconf_t;


static void *create_asp_net_srvconf(apr_pool_t *p, server_rec *s)
{
    asp_net_srvconf_t *srvconf;
    srvconf = (asp_net_srvconf_t*)apr_palloc(p, sizeof(asp_net_srvconf_t));
    srvconf->aliases = apr_hash_make(p);
    return srvconf;
}

static void *merge_asp_net_srvconf(apr_pool_t *p, void *basev, void *addv)
{
    asp_net_srvconf_t *base = (asp_net_srvconf_t*)basev;
    asp_net_srvconf_t *add = (asp_net_srvconf_t*)addv;
    apr_hash_overlay(p, add->aliases, base->aliases);
    return add;
}


static const char *mount_cmd(cmd_parms *cmd, void *dummy,
    const char *uri, const char *rootarg)
{
    char *root;
    asp_net_srvconf_t *srvconf
        = (asp_net_srvconf_t*)ap_get_module_config(cmd->server->module_config,
            &aspdotnet_module);
    asp_net_alias_t *mount
        = (asp_net_alias_t*)apr_palloc(cmd->pool, sizeof(*mount));

    mount->virtual_path_len = strlen(uri);

    apr_status_t rv = apr_filepath_merge((char**)&root, ap_server_root, rootarg,
        APR_FILEPATH_TRUENAME, cmd->pool);

    if (rv != APR_SUCCESS || !root) {
        ap_log_error(APLOG_MARK, APLOG_ERR, rv, cmd->server,
            "mod_aspdotnet: Failed to resolve the full file path"
            "for AspNetMount \"%s\" \"%s\"", uri, rootarg);
        return NULL;
    }

    apr_finfo_t fi;
    rv = apr_stat(&fi, root, APR_FINFO_TYPE, cmd->temp_pool);
    if ((rv != APR_SUCCESS) || (fi.filetype != APR_DIR)) {
        ap_log_error(APLOG_MARK, APLOG_ERR, rv, cmd->server,
            "mod_aspdotnet: File path is not a directory, or could not stat "
            "path for AspNetMount \"%s\" \"%s\"", uri, rootarg);
        return NULL;
    }

    apr_size_t root_len = strlen(root);
    apr_size_t combined_len = mount->virtual_path_len + root_len + 2;
    mount->virtual_path = (const char*)apr_palloc(cmd->pool, combined_len);
    memcpy((char*)mount->virtual_path, uri, mount->virtual_path_len + 1);
    memcpy((char*)mount->virtual_path + mount->virtual_path_len + 1,
        root, root_len + 1);

    // Recover the host matching this virtual+physical path combo, 
    // or create a new persistant host record (on the first pass.)
    mount->host = (asp_net_host_conf_t*)apr_hash_get(global_conf->hosts,
        mount->virtual_path,
        combined_len);
    if (!mount->host) {
        mount->host = (asp_net_host_conf_t*)apr_palloc(global_conf->pool,
            sizeof(*mount->host));
        mount->host->combined_len = combined_len;
        mount->host->virtual_path = (const char*)apr_palloc(global_conf->pool,
            combined_len);
        mount->host->physical_path = mount->host->virtual_path
            + mount->virtual_path_len + 1;
        memcpy((char*)mount->host->virtual_path,
            mount->virtual_path, combined_len);
        mount->host->host_key = -1;
        apr_hash_set(global_conf->hosts, mount->host->virtual_path,
            mount->host->combined_len, mount->host);
    }

    apr_hash_set(srvconf->aliases, mount->virtual_path,
        mount->virtual_path_len, mount);
    return NULL;
}

static const char *version_cmd(cmd_parms *cmd, void *dummy, const char *ver)
{
    global_conf->cor_version = apr_pstrdup(global_conf->pool, ver);
    return NULL;
}


typedef enum {
    ASPNET_OFF = 0,
    ASPNET_UNSET = 2 ^ 0, // Must be a value, better to avoid legal bits
    ASPNET_FILES = 2 ^ 1,
    ASPNET_DIRS = 2 ^ 2,
    ASPNET_VIRTUAL = 2 ^ 3
} asp_net_enable_t;

typedef struct asp_net_dirconf_t {
    int enable;  // Bit flags of asp_net_enable_t values
} asp_net_dirconf_t;

static void *create_asp_net_dirconf(apr_pool_t *p, char *dummy)
{
    asp_net_dirconf_t *dirconf;
    dirconf = (asp_net_dirconf_t*)apr_palloc(p, sizeof(asp_net_dirconf_t));
    dirconf->enable = ASPNET_UNSET;
    return dirconf;
}

static void *merge_asp_net_dirconf(apr_pool_t *p, void *basev, void *addv)
{
    asp_net_dirconf_t *base = (asp_net_dirconf_t*)basev;
    asp_net_dirconf_t *add = (asp_net_dirconf_t*)addv;
    asp_net_dirconf_t *res =
        (asp_net_dirconf_t*)apr_palloc(p, sizeof(asp_net_dirconf_t));
    res->enable = (add->enable == ASPNET_UNSET) ? base->enable
        : add->enable;
    return add;
}

static const char *set_aspnet_enable(cmd_parms *cmd, void *dirconf_,
    const char *enables)
{
    asp_net_dirconf_t *dirconf = (asp_net_dirconf_t *)dirconf_;

    // Unset the AspNet flags before we toggle specific choices
    dirconf->enable = ASPNET_OFF;

    while (*enables) {
        const char *enable = ap_getword_conf(cmd->pool, &enables);

        if (!strcasecmp(enable, "on")
            || !strcasecmp(enable, "all"))
            dirconf->enable = ASPNET_FILES | ASPNET_DIRS | ASPNET_VIRTUAL;
        else if (!strcasecmp(enable, "off")
            || !strcasecmp(enable, "none"))
            dirconf->enable = ASPNET_OFF;
        else if (!strcasecmp(enable, "files"))
            dirconf->enable |= ASPNET_FILES;
        else if (!strcasecmp(enable, "dirs")
            || !strcasecmp(enable, "directories"))
            dirconf->enable |= ASPNET_DIRS;
        else if (!strcasecmp(enable, "virtual"))
            dirconf->enable |= ASPNET_VIRTUAL;
        else {
            return apr_pstrcat(cmd->pool, "Unrecognized AspNet option ",
                enable, NULL);
        }
    }
    return NULL;
}


static const command_rec asp_net_cmds[] =
{
    AP_INIT_RAW_ARGS("AspNet", (cmd_func)set_aspnet_enable, NULL, OR_OPTIONS,
        "Allow ASP.NET to serve 'Files', 'Dirs', 'Virtual', 'All', or 'None'"),
    AP_INIT_TAKE1("AspNetVersion", (cmd_func)version_cmd, NULL, RSRC_CONF,
        "Select a specific ASP.NET version in the format v#.#.####"),
    AP_INIT_TAKE2("AspNetMount", (cmd_func)mount_cmd, NULL, RSRC_CONF,
        "Mount a URI path to an ASP.NET AppDomain root directory"),
    {NULL}
};

static apr_status_t asp_net_stop(void *dummy)
{
    if (global_conf->pHostFactory) {
        global_conf->pHostFactory->Destroy();

        global_conf->pHostFactory->Release();
    }
    if (global_conf->pCorRuntime) {
        global_conf->pCorRuntime->Stop();
        global_conf->pCorRuntime->Release();
    }

    // Although this module should remove itself, we end up in a broken
    // state by unloading ourself underneath the running code.
    //
    // if (global_conf->lock_module)
    //    FreeLibrary(global_conf->lock_module);

    return APR_SUCCESS;
}

static void asp_module_lock98(server_rec *global_server)
{
    // Get the path to this mod_aspdotnet module so that we can
    // lock ourselves in-place for the lifetime of the server
    char AspNetPath[APR_PATH_MAX];
    HMODULE hModule = GetModuleHandleA("mod_aspdotnet.so");
    if (!GetModuleFileNameA(hModule, AspNetPath, APR_PATH_MAX)) {
        HRESULT hr = GetLastError();
        ap_log_error(APLOG_MARK, APLOG_ERR, APR_FROM_OS_ERROR(hr), global_server,
            "mod_aspdotnet: Failed to discover the full file path to "
            "mod_aspdotnet.so.  (Was it renamed?)");
        _com_raise_error(hr);
    }

    // Lock in our module by incrementing its refcount.
    global_conf->lock_module = LoadLibraryA(AspNetPath);
}

static void asp_module_lock(server_rec *global_server)
{
    // Get the path to this mod_aspdotnet module so that we can
    // lock ourselves in-place for the lifetime of the server
    wchar_t wAspNetPath[APR_PATH_MAX];
    HMODULE hModule = GetModuleHandleW(L"mod_aspdotnet.so");
    if (!GetModuleFileNameW(hModule, wAspNetPath, APR_PATH_MAX)) {
        HRESULT hr = GetLastError();
        ap_log_error(APLOG_MARK, APLOG_ERR, APR_FROM_OS_ERROR(hr), global_server,
            "mod_aspdotnet: Failed to discover the full file path to "
            "mod_aspdotnet.so.  (Was it renamed?)");
        _com_raise_error(hr);
    }

    // Lock in our module by incrementing its refcount.
    global_conf->lock_module = LoadLibraryW(wAspNetPath);
}

static void init_runtime_version(asp_net_conf_t *globalconf, wchar_t *runtimeVersion)
{
    int len = (wcslen(runtimeVersion) + 1) * sizeof(wchar_t);

    globalconf->cor_version = (char *)apr_palloc(globalconf->pool, len);
    WideCharToMultiByte(CP_UTF8, 0, runtimeVersion, -1, globalconf->cor_version, len, NULL, NULL);
}

static HRESULT init_clr4_runtime(asp_net_conf_t *globalconf, ICLRMetaHost *pMetaHost)
{
    HRESULT hr = S_OK;
    ICLRRuntimeInfo *pRuntimeInfo = nullptr;
    wchar_t *runtimeVersion = L"v4.0.30319";

    do
    {
        hr = pMetaHost->GetRuntime(L"v4.0.30319", IID_PPV_ARGS(&pRuntimeInfo));
        if (FAILED(hr))
        {
            break;
        }

        hr = pRuntimeInfo->GetInterface(CLSID_CorRuntimeHost, IID_PPV_ARGS(&globalconf->pCorRuntime));
        if (FAILED(hr))
        {
            break;
        }

        init_runtime_version(globalconf, runtimeVersion);
    } while (false);

    if (pRuntimeInfo != nullptr)
    {
        pRuntimeInfo->Release();
    }

    return hr;
}

static HRESULT init_clr2_runtime(asp_net_conf_t *globalconf)
{
    HRESULT hr;

    wchar_t *pszFlavor = L"svr";
    wchar_t *pszVersion = L"v2.0.50727";

    hr = CorBindToRuntimeEx(
        pszVersion,                     // Runtime version 
        pszFlavor,                      // Flavor of the runtime to request 
        0,                              // Runtime startup flags 
        CLSID_CorRuntimeHost,           // CLSID of ICorRuntimeHost 
        IID_PPV_ARGS(&globalconf->pCorRuntime)  // Return ICorRuntimeHost 
    );

    if (FAILED(hr))
    {
        return hr;
    }

    init_runtime_version(globalconf, pszVersion);

    return hr;
}

static HRESULT init_clr_runtime(asp_net_conf_t *globalconf)
{
    ICLRMetaHost *pMetaHost = nullptr;

    HRESULT hr = CLRCreateInstance(CLSID_CLRMetaHost, IID_PPV_ARGS(&pMetaHost));

    if (SUCCEEDED(hr) == true)
    {
        hr = init_clr4_runtime(globalconf, pMetaHost);
    }
    else
    {
        hr = init_clr2_runtime(globalconf);
    }

    if (pMetaHost != nullptr)
    {
        pMetaHost->Release();
    }

    return hr;
}

static HRESULT init_asp_engine(server_rec *global_server)
{
    global_conf->osver.dwOSVersionInfoSize = sizeof(global_conf->osver);
    GetVersionEx(&(global_conf->osver));

    if (global_conf->osver.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS)
        asp_module_lock98(global_server);
    else
        asp_module_lock(global_server);

    // Now we are prepared to register our cleanup in the global
    // process pool, because we trust the module cannot be unloaded
    // by apr_dso_unload [the module instance is refcounted]
    apr_pool_cleanup_register(global_conf->pool, NULL, asp_net_stop,
        apr_pool_cleanup_null);

    ap_log_error(APLOG_MARK, APLOG_DEBUG | APLOG_NOERRNO, 0, global_server,
        "mod_aspdotnet: Module initialization commencing...");

    HRESULT hr = init_clr_runtime(global_conf);

    ap_log_error(APLOG_MARK, APLOG_NOTICE | APLOG_NOERRNO, 0, global_server,
        "mod_aspdotnet: It has loaded version "
        "%s of the .NET CLR engine.", global_conf->cor_version);

    hr = global_conf->pCorRuntime->Start();
    if (FAILED(hr)) {
        ap_log_error(APLOG_MARK, APLOG_ERR, APR_FROM_OS_ERROR(hr), global_server,
            "mod_aspdotnet: Could not start the "
            ".NET CLR engine.");
        _com_raise_error(hr);
    }

    ap_log_error(APLOG_MARK, APLOG_DEBUG | APLOG_NOERRNO, 0, global_server,
        "mod_aspdotnet: Module started .NET CLR...");

    IUnknown *pAppDomainIUnk = NULL;
    hr = global_conf->pCorRuntime->GetDefaultDomain(&pAppDomainIUnk);
    if (FAILED(hr)) {
        ap_log_error(APLOG_MARK, APLOG_ERR, APR_FROM_OS_ERROR(hr), global_server,
            "mod_aspdotnet: Could not retrieve the .NET default "
            "application domain.");
        _com_raise_error(hr);
    }
    if (!pAppDomainIUnk) {
        ap_log_error(APLOG_MARK, APLOG_ERR, APR_FROM_OS_ERROR(hr), global_server,
            "mod_aspdotnet: Could not retrieve the .NET default "
            "application domain's interface.");
        _com_raise_error(E_NOINTERFACE);
    }

    _AppDomain *pDefaultDomain = NULL;
    hr = pAppDomainIUnk->QueryInterface(__uuidof(_AppDomain),
        (void**)&pDefaultDomain);
    // Done with pAppDomainIUnk
    pAppDomainIUnk->Release();

    if (FAILED(hr)) {
        ap_log_error(APLOG_MARK, APLOG_ERR, APR_FROM_OS_ERROR(hr), global_server,
            "mod_aspdotnet: Could not retrieve the .NET default "
            "application domain's _AppDomain interface.");
        _com_raise_error(hr);
    }
    if (!pDefaultDomain) {
        ap_log_error(APLOG_MARK, APLOG_ERR, APR_FROM_OS_ERROR(hr), global_server,
            "mod_aspdotnet: Could not retrieve the .NET default "
            "application domain _AppDomain interface.");
        _com_raise_error(E_NOINTERFACE);
    }

    ap_log_error(APLOG_MARK, APLOG_DEBUG | APLOG_NOERRNO, 0, global_server,
        "mod_aspdotnet: Module initialized .NET default AppDomain...");

    _ObjectHandle *pObjHandle = NULL;
    hr = pDefaultDomain->CreateInstance(_bstr_t("Apache.Web, "
        "Version=" APACHE_WEB_VERSION ", "
        "Culture=neutral, "
        "PublicKeyToken=" APACHE_KEY_TOKEN),
        _bstr_t(L"Apache.Web.HostFactory"),
        &pObjHandle);
    // Done with pDefaultDomain
    pDefaultDomain->Release();

    if (FAILED(hr)) {
        ap_log_error(APLOG_MARK, APLOG_ERR, APR_FROM_OS_ERROR(hr), global_server,
            "mod_aspdotnet: Could not create the .NET interface "
            "for the Apache.Web.HostFactory.");
        _com_raise_error(hr);
    }
    if (!pObjHandle) {
        ap_log_error(APLOG_MARK, APLOG_ERR, APR_FROM_OS_ERROR(hr), global_server,
            "mod_aspdotnet: Could not retrieve the .NET object "
            "for the Apache.Web.HostFactory.");
        _com_raise_error(E_NOINTERFACE);
    }

    VARIANT vHostFactory;
    VariantInit(&vHostFactory);
    hr = pObjHandle->Unwrap(&vHostFactory);
    if (FAILED(hr)) {
        ap_log_error(APLOG_MARK, APLOG_ERR, APR_FROM_OS_ERROR(hr), global_server,
            "mod_aspdotnet: Could not unwrap the COM interface "
            "for the Apache.Web.HostFactory.");
        _com_raise_error(hr);
    }
    if (!vHostFactory.pdispVal) {
        ap_log_error(APLOG_MARK, APLOG_ERR, APR_FROM_OS_ERROR(hr), global_server,
            "mod_aspdotnet: Could not retrieve the unknown COM "
            "interface for the Apache.Web.HostFactory.");
        _com_raise_error(E_NOINTERFACE);
    }

    hr = vHostFactory.pdispVal->QueryInterface(__uuidof(IApacheWebHostFactory),
        (void**)&global_conf->pHostFactory);
    // Done with vHostFactory
    VariantClear(&vHostFactory);

    if (FAILED(hr)) {
        ap_log_error(APLOG_MARK, APLOG_ERR, APR_FROM_OS_ERROR(hr), global_server,
            "mod_aspdotnet: Could not retrieve the COM interface "
            "for the Apache.Web.HostFactory.");
        _com_raise_error(hr);
    }

    ap_log_error(APLOG_MARK, APLOG_DEBUG | APLOG_NOERRNO, 0, global_server,
        "mod_aspdotnet: Module initialized HostFactory...");

    // Test invocation, assure we have a good hostfactory
    hr = global_conf->pHostFactory->Configure(APLOG_MODULE_INDEX, L"");
    if (FAILED(hr)) {
        ap_log_error(APLOG_MARK, APLOG_ERR, APR_FROM_OS_ERROR(hr), global_server,
            "mod_aspdotnet: Could not correctly configure the "
            "Apache.Web.HostFactory.");
        _com_raise_error(hr);
    }

    ap_log_error(APLOG_MARK, APLOG_DEBUG | APLOG_NOERRNO, 0, global_server,
        "mod_aspdotnet: HostFactory configuration complete.");

    return S_OK;
}

static int asp_net_handler(request_rec *r)
{
    if (!r->handler || strcmp(r->handler, "asp.net") != 0) {
        return DECLINED;
    }

    if (!r->uri || (r->uri[0] != '/')) {
        ap_log_rerror(APLOG_MARK, APLOG_ERR | APLOG_NOERRNO, 0, r,
            "mod_aspdotnet: Forbidden, request URI was not absolute: %s",
            r->uri ? r->uri : "<NULL>");
        return HTTP_FORBIDDEN;
    }

    asp_net_dirconf_t *dirconf =
        (asp_net_dirconf_t *)ap_get_module_config(r->per_dir_config,
            &aspdotnet_module);

    if (r->finfo.filetype == APR_REG) {
        if (!(dirconf->enable & ASPNET_FILES))
        {
            ap_log_rerror(APLOG_MARK, APLOG_ERR | APLOG_NOERRNO, 0, r,
                "mod_aspdotnet: Forbidden, 'AspNet Files' is not enabled for: %s"
                " (file %s)", r->uri, r->filename);
            return HTTP_FORBIDDEN;
        }
    }
    else if (r->finfo.filetype == APR_DIR) {
        if (!(dirconf->enable & ASPNET_DIRS))
        {
            ap_log_rerror(APLOG_MARK, APLOG_ERR | APLOG_NOERRNO, 0, r,
                "mod_aspdotnet: Forbidden, 'AspNet Dirs' is not enabled for: %s"
                " (directory %s)", r->uri, r->filename);
            return HTTP_FORBIDDEN;
        }
    }
    else if (!r->finfo.filetype) {
        if (!(dirconf->enable & ASPNET_VIRTUAL))
        {
            ap_log_rerror(APLOG_MARK, APLOG_ERR | APLOG_NOERRNO, 0, r,
                "mod_aspdotnet: Forbidden, 'AspNet Virtual'"
                "is not enabled for: %s", r->uri);
            if (dirconf->enable & (ASPNET_FILES | ASPNET_DIRS))
            {
                ap_log_rerror(APLOG_MARK, APLOG_ERR | APLOG_NOERRNO, 0, r,
                    "mod_aspdotnet: File not found or unable to stat: %s",
                    r->filename);
            }
            return HTTP_NOT_FOUND;
        }
    }
    else {
        ap_log_rerror(APLOG_MARK, APLOG_ERR | APLOG_NOERRNO, 0, r,
            "mod_aspdotnet: Invalid file type for: %s (it is not "
            "a recognized file or directory)", r->filename);
        return HTTP_NOT_FOUND;
    }

    if ((r->used_path_info == AP_REQ_REJECT_PATH_INFO) &&
        r->path_info && *r->path_info)
    {
        /* default to accept */
        ap_log_rerror(APLOG_MARK, APLOG_ERR | APLOG_NOERRNO, 0, r,
            "mod_aspdotnet: AcceptPathInfo off for %s disallows user's path %s",
            r->filename, r->path_info);
        return HTTP_NOT_FOUND;
    }

    asp_net_srvconf_t *srvconf
        = (asp_net_srvconf_t*)ap_get_module_config(r->server->module_config,
            &aspdotnet_module);

    asp_net_host_conf_t *host = NULL;
    apr_hash_index_t *item;
    for (item = apr_hash_first(r->pool, srvconf->aliases); item;
        item = apr_hash_next(item))
    {
        asp_net_alias_t *alias;
        apr_hash_this(item, NULL, NULL, (void**)&alias);
        if (strncasecmp(alias->virtual_path, r->uri,
            alias->virtual_path_len) == 0) {
            host = alias->host;
            break;
        }
    }

    if (!item || !host) {
        ap_log_rerror(APLOG_MARK, APLOG_ERR | APLOG_NOERRNO, 0, r,
            "mod_aspdotnet: No AspNetMount URI for request: %s",
            r->uri);
        return HTTP_INTERNAL_SERVER_ERROR;
    }

    // We never create an environment, however, we will query the
    // r->env to recover these values.
    ap_add_common_vars(r);
    ap_add_cgi_vars(r);

    // Check that THIS thread is coinitialized.
    HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (hr == RPC_E_CHANGED_MODE)
        hr = CoInitialize(NULL);

    if (FAILED(hr)) {
        ap_log_rerror(APLOG_MARK, APLOG_ERR, APR_FROM_OS_ERROR(hr), r,
            "mod_aspdotnet: CoInitialize failed!");
        return HTTP_INTERNAL_SERVER_ERROR;
    }

    VARIANT vRequest;
    VariantInit(&vRequest);

    int status;

    try {
        status = global_conf->pHostFactory->HandleHostRequest(host->host_key,
            (UINT_PTR)r);
    }
    catch (_com_error err) {
        ap_log_rerror(APLOG_MARK, APLOG_ERR, APR_FROM_OS_ERROR(err.Error()), r,
            "mod_aspdotnet: CreateRequest %s processing failed, file %s",
            r->uri, r->filename);
        char *desc = (char*)err.Description();
        if (desc) {
            ap_log_rerror(APLOG_MARK, APLOG_ERR,
                APR_FROM_OS_ERROR(err.Error()), r,
                "mod_aspdotnet: %s", desc);
        }
        status = HTTP_INTERNAL_SERVER_ERROR;
    }
    catch (HRESULT hr) {
        ap_log_rerror(APLOG_MARK, APLOG_ERR, APR_FROM_OS_ERROR(hr), r,
            "mod_aspdotnet: CreateRequest %s processing failed, file %s",
            r->uri, r->filename);
        status = HTTP_INTERNAL_SERVER_ERROR;
    }
    catch (...) {
        ap_log_rerror(APLOG_MARK, APLOG_ERR | APLOG_NOERRNO, 0, r,
            "mod_aspdotnet: CreateRequest %s processing failed, file %s",
            r->uri, r->filename);
        status = HTTP_INTERNAL_SERVER_ERROR;
    }

    VariantClear(&vRequest);

    return status;
}

static int asp_net_pre_config(apr_pool_t *pconf, apr_pool_t *plog,
    apr_pool_t *ptemp)
{
    apr_pool_t *process = apr_pool_parent_get(pconf);

    // Recover or create the global, persistant configuration
    // Setup and teardown can take a considerable amount of time,
    // we do not want to repeat this twice per Apache process.
    if ((apr_pool_userdata_get((void**)&global_conf, "mod_aspdotnet::global_conf",
        process) != APR_SUCCESS) || !global_conf) {
        global_conf = (asp_net_conf_t*)apr_pcalloc(process, sizeof(*global_conf));
        global_conf->pool = process;
        global_conf->hosts = apr_hash_make(process);
        global_conf->pCorRuntime = NULL;
        global_conf->pHostFactory = NULL;

        apr_pool_userdata_setn(global_conf, "mod_aspdotnet::global_conf",
            apr_pool_cleanup_null, process);
    }
    return APR_SUCCESS;
}


static int asp_net_post_config(apr_pool_t *pconf, apr_pool_t *plog,
    apr_pool_t *ptemp, server_rec *global_server)
{
    apr_hash_index_t *item;

    // Decorate the server string
    ap_add_version_component(pconf, "mod_aspdotnet/"
        APW_STRINGIFY(APACHE_WEB_VER_MAJOR) "."
        APW_STRINGIFY(APACHE_WEB_VER_MINOR));

    // First time through, initialize .Net and the HostFactory
    if (!global_conf->pCorRuntime || !global_conf->pHostFactory) {
        HRESULT hr = CoInitialize(NULL);
        if (hr == RPC_E_CHANGED_MODE)
            hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
        if (FAILED(hr)) {
            ap_log_error(APLOG_MARK, APLOG_CRIT, APR_FROM_OS_ERROR(hr), global_server,
                "mod_aspdotnet: Failed to CoInitialize the threaded "
                "COM engine for .NET CLR interop!");
            return 1;
        }
        try {
            init_asp_engine(global_server);
        }
        catch (_com_error err) {
            char *desc = (char*)err.Description();
            ap_log_error(APLOG_MARK, APLOG_CRIT,
                APR_FROM_OS_ERROR(err.Error()), global_server,
                "mod_aspdotnet: Failed to start Asp.Net "
                "Apache.Web host factory");
            if (desc) {
                ap_log_error(APLOG_MARK, APLOG_CRIT,
                    APR_FROM_OS_ERROR(err.Error()), global_server,
                    "mod_aspdotnet: %s", desc);
            }
            return 1;
        }
        catch (HRESULT hr) {
            ap_log_error(APLOG_MARK, APLOG_ERR, APR_FROM_OS_ERROR(hr), global_server,
                "mod_aspdotnet: Failed to start Asp.Net "
                "Apache.Web host factory.");
            return 1;
        }
        catch (...) {
            ap_log_error(APLOG_MARK, APLOG_ERR | APLOG_NOERRNO, 0, global_server,
                "mod_aspdotnet: Failed to start Asp.Net "
                "Apache.Web host factory.");
            return 1;
        }
    }

    for (item = apr_hash_first(ptemp, global_conf->hosts); item;
        item = apr_hash_next(item))
    {
        wchar_t *path;
        apr_size_t inlen, outlen;
        _bstr_t virtual_path, physical_path;
        asp_net_host_conf_t *host;
        apr_hash_this(item, NULL, NULL, (void**)&host);

        // Initialize each host mapping, only once.
        if (host->host_key != -1) {
            continue;
        }

        // Treat AspNetMount paths as utf-8 strings, fail over to
        // default _bstr_t multibyte conversion if unconvertable.
        // Relies on simple utf-8 -> unicode rule, that output will
        // never be more wchar's than the number of input chars.
        inlen = strlen(host->virtual_path) + 1;
        path = (wchar_t*)apr_palloc(ptemp, inlen * 2);
        outlen = MultiByteToWideChar(CP_UTF8, 0, host->virtual_path,
            inlen, path, inlen);
        if (outlen) {
            virtual_path = path;
        }
        else {
            virtual_path = host->virtual_path;
        }

        inlen = strlen(host->physical_path) + 1;
        path = (wchar_t*)apr_palloc(ptemp, inlen * 2);
        outlen = MultiByteToWideChar(CP_UTF8, 0, host->physical_path,
            inlen, path, inlen);
        if (outlen) {
            physical_path = path;
        }
        else {
            physical_path = host->physical_path;
        }

        try {
            host->host_key = global_conf->pHostFactory->CreateHost(
                virtual_path, physical_path, (UINT_PTR)global_server);
            if (host->host_key == -1)
                _com_raise_error(E_NOINTERFACE);
        }
        catch (_com_error err) {
            ap_log_error(APLOG_MARK, APLOG_CRIT,
                APR_FROM_OS_ERROR(err.Error()), global_server,
                "mod_aspdotnet: Failed to create Host connector "
                "for %s mapped to %s", host->virtual_path,
                host->physical_path);
            ap_log_error(APLOG_MARK, APLOG_CRIT,
                APR_FROM_OS_ERROR(err.Error()), global_server,
                "mod_aspdotnet: %s", (char*)err.Description());
            return 1;
        }
        catch (HRESULT hr) {
            ap_log_error(APLOG_MARK, APLOG_ERR, APR_FROM_OS_ERROR(hr), global_server,
                "mod_aspdotnet: Failed to create Host for %s mapped "
                "to %s", host->virtual_path, host->physical_path);
            return 1;
        }
        catch (...) {
            ap_log_error(APLOG_MARK, APLOG_ERR | APLOG_NOERRNO, 0, global_server,
                "mod_aspdotnet: Failed to create Host for %s mapped "
                "to %s", host->virtual_path, host->physical_path);
            return 1;
        }
        ap_log_error(APLOG_MARK, APLOG_DEBUG | APLOG_NOERRNO, 0, global_server,
            "mod_aspdotnet: Sucessfully created Host for %s mapped "
            "to %s", host->virtual_path, host->physical_path);
    }

    return OK;
}

static void register_hooks(apr_pool_t *p)
{
    ap_hook_handler(asp_net_handler, NULL, NULL, APR_HOOK_MIDDLE);
    ap_hook_pre_config(asp_net_pre_config, NULL, NULL, APR_HOOK_MIDDLE);
    ap_hook_post_config(asp_net_post_config, NULL, NULL, APR_HOOK_MIDDLE);
}

module AP_MODULE_DECLARE_DATA::aspdotnet_module =
{
    STANDARD20_MODULE_STUFF,
    create_asp_net_dirconf,     /* dir config */
    merge_asp_net_dirconf,      /* merge dir config */
    create_asp_net_srvconf,     /* server config */
    merge_asp_net_srvconf,      /* merge server config */
    asp_net_cmds,               /* command apr_table_t */
    register_hooks              /* register hooks */
};
