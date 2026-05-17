#ifndef _USERLIBC_FNMATCH_H
#define _USERLIBC_FNMATCH_H

// Match failed error code
#define FNM_NOMATCH 1

// Flags to control the matching behavior
#define FNM_NOESCAPE 0x01 // Treat backslash '\' as an ordinary character
#define FNM_PATHNAME 0x02 // Slash '/' must be matched explicitly by a slash
#define FNM_PERIOD   0x04 // Leading period '.' must match a period explicitly
#define FNM_CASEFOLD 0x10 // Case-insensitive matching (GNU extension used by BusyBox)

// POSIX function prototype
int fnmatch(const char *pattern, const char *string, int flags);

#endif // _USERLIBC_FNMATCH_H
