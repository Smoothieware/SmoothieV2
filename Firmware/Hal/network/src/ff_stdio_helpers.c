#include "ff_stdio.h"

#include "FreeRTOS.h"

uint32_t getFileSize(FILE *fp)
{
	fseek(fp, 0, SEEK_END);
	uint32_t len = (uint32_t)ftell(fp);
	fseek(fp, 0, SEEK_SET);

	return len;
}

int ff_findnext( FF_FindData_t *pxFindData )
{
    FRESULT res = f_readdir(&pxFindData->dir, &pxFindData->xDirectoryEntry);
    if ((res != FR_OK) || !pxFindData->xDirectoryEntry.fname[0]){
    	f_closedir(&pxFindData->dir);
    	return res;
    }

    pxFindData->ucAttrib= pxFindData->xDirectoryEntry.fattrib;
    pxFindData->ulFileSize= pxFindData->xDirectoryEntry.fsize;
	pxFindData->pcFileName= pxFindData->xDirectoryEntry.fname;
	return 0;
}

int ff_findfirst( const char *pcDirectory, FF_FindData_t *pxFindData )
{
	FRESULT res = f_opendir(&pxFindData->dir, pcDirectory);
	if(FR_OK != res) {
		FF_PRINTF("Could not open directory %s (%d)\n", pcDirectory, res);
		return res;
	}

	return ff_findnext(pxFindData);
}


