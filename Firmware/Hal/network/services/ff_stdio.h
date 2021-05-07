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
#define ff_mkdir(a) f_mkdir(a)
#define ff_rmdir(a) f_unlink(a)
#define ff_rename(a,b,c) f_rename(a, b)
#define ff_remove(a) unlink(a)

#define FF_SEEK_SET SEEK_SET

/* Structure used with ff_findfirst(), ff_findnext(), etc. */
typedef struct
{
    DIR dir;
    FILINFO xDirectoryEntry;

	/* Public fields included so FF_DirEnt_t does not need to be public. */
    const char * pcFileName;
	uint32_t ulFileSize;
	uint8_t ucAttrib;
} FF_FindData_t;

#define FF_FAT_ATTR_DIR AM_DIR
#define FF_FAT_ATTR_READONLY AM_RDO

extern int ff_findfirst( const char *pcDirectory, FF_FindData_t *pxFindData );
extern int ff_findnext( FF_FindData_t *pxFindData );
extern uint32_t getFileSize(FILE *fp);

typedef struct
{
	uint16_t Year;		/* Year	(e.g. 2009). */
	uint16_t Month;		/* Month	(e.g. 1 = Jan, 12 = Dec). */
	uint16_t Day;		/* Day	(1 - 31). */
	uint16_t Hour;		/* Hour	(0 - 23). */
	uint16_t Minute;	/* Min	(0 - 59). */
	uint16_t Second;	/* Second	(0 - 59). */
} FF_SystemTime_t;

#define FF_FS_Count() 1
#define ffconfigMAX_FILENAME FF_MAX_LFN
#define ipconfigTCP_COMMAND_BUFFER_SIZE 2060

#define stdioGET_ERRNO(x) errno

#define ffconfigTIME_SUPPORT 0
#define fconfigDEV_SUPPORT 0
#define ipconfigFTP_FS_USES_BACKSLAH 0
#define ffconfigMKDIR_RECURSIVE 0
#define ffconfigDEV_SUPPORT 0
#ifdef DEBUG
#define FF_PRINTF printf
#else
#define FF_PRINTF(...)
#endif
