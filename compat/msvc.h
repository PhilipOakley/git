#ifndef __MSVC__HEAD
#define __MSVC__HEAD

#include <direct.h>
#include <process.h>
#include <malloc.h>
#include <io.h>

/* porting function */
#define inline __inline
#define __inline__ __inline
#define __attribute__(x)
#define strncasecmp  _strnicmp
#define ftruncate    _chsize
#define strtoull     _strtoui64
#define strtoll      _strtoi64

static __inline int strcasecmp (const char *s1, const char *s2)
{
	int size1 = strlen(s1);
	int sisz2 = strlen(s2);
	return _strnicmp(s1, s2, sisz2 > size1 ? sisz2 : size1);
}

#undef ERROR

#ifdef _MSC_VER
typedef int sigset_t;
#define O_ACCMODE     _O_RDWR  /* open for reading and writing (not in fcntl.h) */

/* https://msdn.microsoft.com/en-us/library/ff552012.aspx */
typedef struct _REPARSE_DATA_BUFFER {
  ULONG  ReparseTag;
  USHORT ReparseDataLength;
  USHORT Reserved;
  union {
    struct {
      USHORT SubstituteNameOffset;
      USHORT SubstituteNameLength;
      USHORT PrintNameOffset;
      USHORT PrintNameLength;
      ULONG  Flags;
      WCHAR  PathBuffer[1];
    } SymbolicLinkReparseBuffer;
    struct {
      USHORT SubstituteNameOffset;
      USHORT SubstituteNameLength;
      USHORT PrintNameOffset;
      USHORT PrintNameLength;
      WCHAR  PathBuffer[1];
    } MountPointReparseBuffer;
    struct {
      UCHAR DataBuffer[1];
    } GenericReparseBuffer;
  };
} REPARSE_DATA_BUFFER, *PREPARSE_DATA_BUFFER;

/* in the absence of the ntifs.h DDK (device driver development kit) */

#endif

#include "compat/mingw.h"

#endif
