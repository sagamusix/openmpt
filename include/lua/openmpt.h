/*
 * openmpt.h
 * ---------
 * Purpose: OpenMPT-specific changes to Lua
 * Notes  : (currently none)
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#ifdef _WIN32

#include <stdio.h>
#include <windows.h>

// We assume strings are UTF-8 and make use of the wide-char APIs in Windows
FILE *openmpt_fopen(const char *filename, const char *mode);
FILE *openmpt_popen(const char *command, const char *mode);
HMODULE openmpt_LoadLibraryExA(LPCSTR lpLibFileName, HANDLE hFile, DWORD dwFlags);

#define fopen(filename, mode) (openmpt_fopen(filename, mode))
#define l_popen(L,c,m) (openmpt_popen(c,m))
#define l_pclose(L,file) (_pclose(file))
#define LoadLibraryExA(lpLibFileName, hFile, dwFlags) (openmpt_LoadLibraryExA(lpLibFileName, hFile, dwFlags))

#endif
