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

// quicksearch.h declarations


// the user-implemented filesearch prototype, e.g.
//
// void cbfunc(char *path, size_t len, WIN32_FILE_ATTRIBUTE_DATA *fi, void *arg)
// {
//     printf("FOUND %s\n", path);
// }
//
typedef void (filesearch)(char *path, size_t len, WIN32_FILE_ATTRIBUTE_DATA *fi, void *arg);

// quicksearch exposes the drsearch mechanism to quickly identify
// and return all occurances of {findfile} within the user's fixed drives.
// 
// All occurances are passed back to the filesearch callback, which can
// identify the full path, the length of the 'root' path (including the
// trailing backslash) and a user defined optional argument.  e.g.
//
//    drsearch("apache\\bin\\apache.exe", 3, cbfunc, NULL);
//
void drsearch(char *findfile, int depth, filesearch *cb, void *arg);

