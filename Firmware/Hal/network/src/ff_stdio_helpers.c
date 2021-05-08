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
		FF_PRINTF("ff_findnext: End of directory\n");
    	f_closedir(&pxFindData->dir);
    	return res == FR_OK ? FR_NO_FILE : res;
    }

    pxFindData->ucAttrib= pxFindData->xDirectoryEntry.fattrib;
    pxFindData->ulFileSize= pxFindData->xDirectoryEntry.fsize;
	pxFindData->pcFileName= pxFindData->xDirectoryEntry.fname;
	FF_PRINTF("ff_findnext: found %s: %04X\n", pxFindData->pcFileName, pxFindData->ucAttrib);
	return 0;
}

int ff_findfirst( const char *pcDirectory, FF_FindData_t *pxFindData )
{
	FRESULT res = f_opendir(&pxFindData->dir, pcDirectory);
	FF_PRINTF("ff_findfirst: open directory %s (%d)\n", pcDirectory, res);
	if(FR_OK != res) {
		FF_PRINTF("ff_findfirst: Could not open directory %s (%d)\n", pcDirectory, res);
		return res;
	}

	return ff_findnext(pxFindData);
}


