/* Includes */
#include <sys/stat.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <signal.h>
#include <time.h>
#include <sys/time.h>
#include <sys/times.h>
#include <fcntl.h>

#include "ff.h"
#include "uart_comms.h"

extern int fatfs_to_errno( FRESULT Result );

#if 1
/*
 * Map newlib calls to fflib
 */

// support routines for mapping file numbers to file handles
#define NFH 10
static FIL* fh_map[NFH]= {NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL};
static int allocate_fh(FIL *fh)
{
    for (int i = 0; i < NFH; ++i) {
        if(fh_map[i] == NULL) {
            fh_map[i] = fh;
            return i+3;
        }
    }
    return -1;
}

static FIL* get_fh(int fn)
{
    if(fn-3 >= NFH) return NULL;
    return fh_map[fn-3];
}

static void deallocate_fh(int fn)
{
    if(fn-3 < NFH) {
        fh_map[fn-3]= NULL;
    }
}

int _getpid(void)
{
	return 1;
}

int _kill(int pid, int sig)
{
	errno = EINVAL;
	return -1;
}

void _exit (int status)
{
	_kill(status, -1);
    __asm("bkpt #0");
	while (1) {}		/* Make sure we hang here */
}

int _open(char *path, int flags, ...)
{
    /* POSIX flags -> FatFS open mode */
    BYTE openmode;
    if(flags & O_RDWR) {
        openmode = FA_READ|FA_WRITE;
    } else if(flags & O_WRONLY) {
        openmode = FA_WRITE;
    } else {
        openmode = FA_READ;
    }
    if(flags & O_CREAT) {
        if(flags & O_TRUNC) {
            openmode |= FA_CREATE_ALWAYS;
        } else {
            openmode |= FA_OPEN_ALWAYS;
        }
    }
    if(flags & O_APPEND) {
        openmode |= FA_OPEN_APPEND;
    }

    FIL *fh= malloc(sizeof(FIL));
    FRESULT res = f_open(fh, path, openmode);
    if(res != FR_OK) {
        free(fh);
        errno= fatfs_to_errno(res);
        return -1;
    }

    // save the fn to fh mapping
    int fn= allocate_fh(fh);
    if(fn < 0) {
        free(fh);
        errno= ENFILE;
        return -1;
    }
    return fn;
}

int _close(int file)
{
    if(file < 3) return 0;

    FIL *fh= get_fh(file);
    if(fh == NULL) {
        errno= EBADF;
        return -1;
    }

    FRESULT res = f_close(fh);
    free(fh);
    deallocate_fh(file);

    if(res != FR_OK) {
        errno= fatfs_to_errno(res);
        return -1;
    }
    return 0;
}

int _write(int file, char *buffer, int length)
{
    if(file < 3) {
        // Note this will block until all sent
        return write_uart(buffer, length);
    }

    FIL *fh= get_fh(file);
    if(fh == NULL) {
        errno= EBADF;
        return -1;
    }

    UINT n;
    FRESULT res = f_write(fh, buffer, length, &n);
    if(res != FR_OK) {
        errno= fatfs_to_errno(res);
        return -1;
    }

    return n;
}

int _read(int file, char *buffer, int length)
{
    if(file < 3) {
        // Note this can return less than request or even 0
        return read_uart(buffer, length);
    }

    FIL *fh= get_fh(file);
    if(fh == NULL) {
        errno= EBADF;
        return -1;
    }

    UINT n;
    FRESULT res = f_read(fh, buffer, length, &n);
    if(res != FR_OK) {
        errno= fatfs_to_errno(res);
        return -1;
    }
    return n;
}

int _fstat(int file, struct stat *st)
{
    if(file < 3) {
       st->st_mode = S_IFCHR;
	   return 0;
    }

    FIL *fh= get_fh(file);
    if(fh == NULL) {
        errno= EBADF;
        return -1;
    }

    st->st_size= f_size(fh);
    st->st_mode= S_IFREG;

    return 0;
}

int _stat(char *file, struct stat *st)
{
    FILINFO fno;
    FRESULT res= f_stat(file, &fno);
    if(res != FR_OK) {
        errno= fatfs_to_errno(res);
        return -1;
    }

    if(fno.fattrib & AM_DIR) {
        st->st_mode= S_IFDIR;
    }else{
        st->st_size= fno.fsize;
        st->st_mode= S_IFREG;
    }

    return 0;
}

int rename(const char *old, const char *new)
{
    FRESULT res= f_rename(old, new);
    if(res != FR_OK) {
        errno= fatfs_to_errno(res);
        return -1;
    }

    return 0;
}

int _isatty(int file)
{
	return (file >= 0 || file <=2) ? 1 : 0;
}

int _lseek(int file, int position, int whence)
{
    if(file < 3) {
       return 0;
    }

    FIL *fh= get_fh(file);
    if(fh == NULL) {
        errno= EBADF;
        return -1;
    }

    if(whence == SEEK_END) {
        position += f_size(fh);
    } else if(whence==SEEK_CUR) {
        position += f_tell(fh);
    } else if(whence!=SEEK_SET) {
        errno= EINVAL;
        return -1;
    }

    FRESULT res = f_lseek(fh, position);
    if(res != FR_OK) {
        errno= fatfs_to_errno(res);
        return -1;
    }

    return position;
}


int _wait(int *status)
{
	errno = ECHILD;
	return -1;
}

int _unlink(char *name)
{
    FRESULT res= f_unlink(name);
    if(res != FR_OK) {
        errno= fatfs_to_errno(res);
        return -1;
    }

	return 0;
}

int _times(struct tms *buf)
{
	return -1;
}

int _link(char *old, char *new)
{
	errno = EMLINK;
	return -1;
}

int _fork(void)
{
	errno = EAGAIN;
	return -1;
}

int _execve(char *name, char **argv, char **env)
{
	errno = ENOMEM;
	return -1;
}
#else

extern int errno;

char *__env[1] = { 0 };
char **environ = __env;


/* Functions */
void initialise_monitor_handles()
{
}

int _getpid(void)
{
    return 1;
}

int _kill(int pid, int sig)
{
    errno = EINVAL;
    return -1;
}

void _exit (int status)
{
    _kill(status, -1);
    while (1) {}        /* Make sure we hang here */
}

int _read (int file, char *ptr, int len)
{
    return read_uart(ptr, len);
}

int _write(int file, char *ptr, int len)
{
    return write_uart(ptr, len);
}

int _close(int file)
{
    return -1;
}


int _fstat(int file, struct stat *st)
{
    st->st_mode = S_IFCHR;
    return 0;
}

int _isatty(int file)
{
    return 1;
}

int _lseek(int file, int ptr, int dir)
{
    return 0;
}

int _open(char *path, int flags, ...)
{
    /* Pretend like we always fail */
    return -1;
}

int _wait(int *status)
{
    errno = ECHILD;
    return -1;
}

int _unlink(char *name)
{
    errno = ENOENT;
    return -1;
}

int _times(struct tms *buf)
{
    return -1;
}

int _stat(char *file, struct stat *st)
{
    st->st_mode = S_IFCHR;
    return 0;
}

int _link(char *old, char *new)
{
    errno = EMLINK;
    return -1;
}

int _fork(void)
{
    errno = EAGAIN;
    return -1;
}

int _execve(char *name, char **argv, char **env)
{
    errno = ENOMEM;
    return -1;
}


#endif
