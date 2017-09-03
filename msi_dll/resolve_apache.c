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

// MSI custom action dll to locate Apache 2.0 installations


#define WINDOWS_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <tchar.h>
#include <ctype.h>
#include <Strsafe.h>
#include "MsiQuery.h"
#include "quicksearch.h"

#define stringize_helper(var) #var
#define stringize(var) stringize_helper(var)

struct cbResolverArg {
    MSIHANDLE hInstall;
    MSIHANDLE hDB;
    MSIHANDLE hView;
    char     *combo;
    char     *prop;
    int       index;
};

void cbResolver(char *path, size_t len, WIN32_FILE_ATTRIBUTE_DATA *fi, void *arg)
{
    struct cbResolverArg *wrk = arg;

    MSIHANDLE rec = MsiCreateRecord(4);
    if (!rec) {
        return;
    }

    // this is safe, everything after path[len-1] is ours to play with.
    path[len] = '\0';

    if (!wrk->index) {
        // First Hit?  We need to set up the property, too!
        MsiSetProperty(wrk->hInstall, "_BadPath", "0");
        MsiSetProperty(wrk->hInstall, wrk->combo, path);
        MsiSetTargetPath(wrk->hInstall, "INSTALLDIR", path);
    }

    // Modify the ComboBox Control
    MsiRecordSetString (rec, 1, wrk->combo);
    MsiRecordSetInteger(rec, 2, ++wrk->index);
    MsiRecordSetString (rec, 3, path);
    MsiRecordSetString (rec, 4, NULL);

    if (MsiViewModify(wrk->hView, MSIMODIFY_INSERT_TEMPORARY, rec) == ERROR_SUCCESS) {
        MsiViewExecute(wrk->hView, rec);
    }

    MsiCloseHandle(rec);
}

UINT __declspec(dllexport) __stdcall ResolveApacheInstances(MSIHANDLE hInstall) 
{
    UINT res;
    struct cbResolverArg wrk;

    wrk.index = 0;
    wrk.combo = "INSTALLDIR";
    wrk.hInstall = hInstall;
    wrk.hDB = MsiGetActiveDatabase(wrk.hInstall);

    if (!wrk.hDB) {
        return ERROR_SUCCESS;
    }

    MsiSetProperty(hInstall, "_BadPath", "1");

    res = MsiDatabaseOpenView(wrk.hDB, "SELECT * FROM ComboBox", &wrk.hView);
    if (res != ERROR_SUCCESS) {
        MsiCloseHandle(wrk.hDB);
        return res;
    }

    drsearch("bin\\" stringize(BINARY), 3, cbResolver, &wrk);

    MsiCloseHandle(wrk.hView);
    MsiCloseHandle(wrk.hDB);
    return ERROR_SUCCESS;
}

UINT tryAddPath(MSIHANDLE hInstall, char *str, int len, WIN32_FILE_ATTRIBUTE_DATA *fi)
{
    char query[1024];
    struct cbResolverArg wrk;
    MSIHANDLE rec = 0;
    UINT res;
    
    wrk.index = 0;
    wrk.combo = "INSTALLDIR";
    wrk.hInstall = hInstall;
    wrk.hDB = MsiGetActiveDatabase(wrk.hInstall);

    str[len] = '\0';

    if (!wrk.hDB) {
        return ERROR_SUCCESS;
    }

    MsiSetProperty(hInstall, "_BadPath", "1");

    StringCchPrintfA(query, 1024, "SELECT * FROM ComboBox WHERE Property = \"%s\" AND Value = \"%s\"",
            wrk.combo, str);
    res = MsiDatabaseOpenView(wrk.hDB, query, &wrk.hView);
    if (res != ERROR_SUCCESS) {
        MsiCloseHandle(wrk.hDB);
        return res;
    }

    res = MsiViewFetch(wrk.hView, &rec);
    if (res == ERROR_SUCCESS) {
        if (rec) {
            MsiCloseHandle(rec);
        }
    }
    else if (res == ERROR_NO_MORE_ITEMS) {
        // Actually insert the record, if not found.
        cbResolver(str, len, fi, &wrk);
    }

    MsiCloseHandle(wrk.hView);
    MsiCloseHandle(wrk.hDB);
    return ERROR_SUCCESS;
}

UINT __declspec(dllexport) __stdcall VerifyApachePath(MSIHANDLE hInstall) 
{
    char chkfile[] = "bin\\" stringize(BINARY);
    WIN32_FILE_ATTRIBUTE_DATA fi;
    DWORD len = MAX_PATH;
    char str[MAX_PATH + 1];

    if ((MsiGetProperty(hInstall, "INSTALLDIR", str, &len) != ERROR_SUCCESS)
            || (len + sizeof(chkfile) > MAX_PATH)) {
        MsiSetProperty(hInstall, "_BadPath", "1");
        return ERROR_SUCCESS;
    }

    len = strlen(str);

    if (str[len - 1] != '\\') {
        str[len++] = '\\';
        str[len] = '\0';
        MsiSetProperty(hInstall, "INSTALLDIR", str);
    }

    strcpy_s(str + len, MAX_PATH, chkfile);
    if (!GetFileAttributesEx(str, GetFileExInfoStandard, &fi)) {
        MsiSetProperty(hInstall, "_BadPath", "1");
        return ERROR_SUCCESS;
    }
    str[len] = '\0';

    MsiSetTargetPath(hInstall, "INSTALLDIR", str);

    tryAddPath(hInstall, str, len, &fi);

    MsiSetProperty(hInstall, "_BadPath", "0");
    return ERROR_SUCCESS;
}
