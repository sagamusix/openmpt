/*
 * openmpt.c
 * ---------
 * Purpose: OpenMPT-specific changes to Lua
 * Notes  : (currently none)
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#ifdef _WIN32

#include <windows.h>
#include <stdio.h>

static wchar_t *Utf8ToWide(const char *utf8string)
{
	int required_size = MultiByteToWideChar(CP_UTF8, 0, utf8string, -1, NULL, 0);
	if(required_size <= 0) return NULL;
	wchar_t *decoded_string = (wchar_t *)calloc(required_size, sizeof(wchar_t));
	MultiByteToWideChar(CP_UTF8, 0, utf8string, -1, decoded_string, required_size);
	return decoded_string;
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

#endif
