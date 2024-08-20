/*
 * openmpt.c
 * ---------
 * Purpose: OpenMPT-specific changes to Lua
 * Notes  : (currently none)
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#ifdef _WIN32

#include <stdio.h>
#include <windows.h>

static wchar_t *Utf8ToWide(const char *utf8string)
{
	int requiredSize = MultiByteToWideChar(CP_UTF8, 0, utf8string, -1, NULL, 0);
	if(requiredSize <= 0)
		return NULL;
	wchar_t *decodedString = (wchar_t *)calloc(requiredSize, sizeof(wchar_t));
	MultiByteToWideChar(CP_UTF8, 0, utf8string, -1, decodedString, requiredSize);
	return decodedString;
}

// We assume strings are UTF-8 and make use of the wide-char APIs in Windows
FILE *openmpt_fopen(const char *filename, const char *mode)
{
	wchar_t *filenameW = Utf8ToWide(filename), *modeW = Utf8ToWide(mode);
	FILE *ret = _wfopen(filenameW, modeW);
	free(filenameW);
	free(modeW);
	return ret;
}

// We assume strings are UTF-8 and make use of the wide-char APIs in Windows
FILE *openmpt_popen(const char *command, const char *mode)
{
	wchar_t *commandW = Utf8ToWide(command), *modeW = Utf8ToWide(mode);
	FILE *ret = _wpopen(commandW, modeW);
	free(commandW);
	free(modeW);
	return ret;
}

// We assume strings are UTF-8 and make use of the wide-char APIs in Windows
HMODULE openmpt_LoadLibraryExA(LPCSTR lpLibFileName, HANDLE hFile, DWORD dwFlags)
{
	wchar_t *filenameW = Utf8ToWide(lpLibFileName);
	HMODULE ret = LoadLibraryExW(filenameW, hFile, dwFlags);
	free(filenameW);
	return ret;
}

#endif
