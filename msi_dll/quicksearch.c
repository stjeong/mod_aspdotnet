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

#define _WIN32_WINNT 0x0400
#include <windows.h>
#include "quicksearch.h"

void volsearch(char *curpath, size_t pathlen, char *findfile, size_t findlen, 
               int depth, filesearch *cb, void *arg)
{
    WIN32_FILE_ATTRIBUTE_DATA fi;
    WIN32_FIND_DATA found;
    HANDLE dh;

    if (pathlen > 3) {
        curpath[pathlen++] = '\\';
    }

    memcpy(curpath + pathlen, findfile, findlen + 1);

    if (GetFileAttributesEx(curpath, GetFileExInfoStandard, &fi)) {
        cb(curpath, pathlen, &fi, arg);
    }

    if (!depth) {
        curpath[pathlen] = '\0';
        return;
    }

    if (pathlen == 3) {
        curpath[pathlen] = '*';
        curpath[pathlen + 1] = '\0';
    }
    else {
        curpath[pathlen] = '*';
        curpath[pathlen + 1] = '\0';
    }

    dh = FindFirstFileEx(curpath, FindExInfoStandard, &found, 
                         FindExSearchLimitToDirectories, NULL, 0);

    while (dh != INVALID_HANDLE_VALUE) {
        size_t namelen = strlen(found.cFileName);

        if ((found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            && ((found.cFileName[0] != '.')
                || (found.cFileName[1] 
                    && ((found.cFileName[1] != '.') || found.cFileName[2])))
            && (pathlen + namelen + findlen + 1 <= MAX_PATH)) {

            memcpy(curpath + pathlen, found.cFileName, namelen + 1);
            volsearch(curpath, pathlen + namelen, findfile, findlen, depth - 1, cb, arg);
        }

        if (!FindNextFile(dh, &found)) {
            FindClose(dh);
            dh = INVALID_HANDLE_VALUE;
        }
    }

    curpath[pathlen] = '\0';
}


void drsearch(char *findfile, int depth, filesearch *cb, void *arg)
{
    DWORD drives;
    char search[MAX_PATH + 1];

    search[0] = 'A';
    search[1] = ':';
    search[2] = '\\';

    drives = GetLogicalDrives();
    while (drives) {
        if (drives & 1) {
            search[3] = '\0';
            if (GetDriveType(search) == DRIVE_FIXED) {
                volsearch(search, 3, findfile, strlen(findfile), depth, cb, arg);
            }
        }
        drives >>= 1;
        ++search[0];
    }
}
