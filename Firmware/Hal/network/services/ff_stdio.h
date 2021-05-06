// map ff stdio to stdio
#pragma once

#include <stdio.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

// from fatlib
#include "ff.h"

#define FF_FILE FILE

#define ff_fopen fopen
#define ff_fclose fclose
#define ff_fseek fseek
#define ff_rewind rewind
#define ff_ftell ftell
#define ff_feof feof
#define ff_fread fread
#define ff_fwrite fwrite
#define ff_mkdir(a) f_mkdir(a, 0)
#define ff_rmdir(a) f_unlink(a)
#define ff_rename(a,b,c) f_rename(a, b)
#define ff_remove(a) unlink(a)

#define ff_findfirst(a, b) (-1)
#define ff_findnext(a) (-1)
#define ff_finddir(a) (0)

#define FF_SEEK_SET SEEK_SET

typedef struct
{
	uint16_t Year;		/* Year	(e.g. 2009). */
	uint16_t Month;		/* Month	(e.g. 1 = Jan, 12 = Dec). */
	uint16_t Day;		/* Day	(1 - 31). */
	uint16_t Hour;		/* Hour	(0 - 23). */
	uint16_t Minute;	/* Min	(0 - 59). */
	uint16_t Second;	/* Second	(0 - 59). */
} FF_SystemTime_t;

#define ffconfigMAX_FILENAME FF_MAX_LFN

#define stdioGET_ERRNO(x) errno

#define ffconfigTIME_SUPPORT 0
#define fconfigDEV_SUPPORT 0
#define ipconfigFTP_FS_USES_BACKSLAH 0
#define ffconfigMKDIR_RECURSIVE 0

#define FF_PRINTF printf
