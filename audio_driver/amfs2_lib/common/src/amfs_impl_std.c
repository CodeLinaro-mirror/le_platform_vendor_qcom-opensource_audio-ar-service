/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/stat.h>
#include "amfs.h"
#include "amfs_util.h"
#include "amfs_impl_std.h"

#ifndef AMFS_ASSERT
#define AMFS_ASSERT(test) \
    switch(0) { \
        case 0: \
        case test:; \
}
#endif //AMFS_ASSERT

#define AMFS_STD_STREAM_FILE        1
#define AMFS_STD_STREAM_BUF         2

#define ERRNO (errno == 0 ? -1 : errno)

#define AMFS_BASE_PATH "/vendor/dsp"
#define combpath(x,y) (x#y)
/* Default enviroment setting */
#ifdef ENABLE_AUDIO_FRONTEND /* Front End */
static char *ADSP_LIBRARY_PATH = combpath(AMFS_BASE_PATH,/adsp);
static char *GPDSP0_LIBRARY_PATH = combpath(AMFS_BASE_PATH,/gpdsp);
static char *GPDSP1_LIBRARY_PATH = combpath(AMFS_BASE_PATH,/gpdsp1);
static char *CDSP0_LIBRARY_PATH = combpath(AMFS_BASE_PATH,/cdsp);
static char *CDSP1_LIBRARY_PATH = combpath(AMFS_BASE_PATH,/cdsp1);
static char *COMMON_DSP_LIBRARY_PATH = "/usr/lib/rfsa/adsp";
#else /* Back End */
static char *ADSP_LIBRARY_PATH = combpath(AMFS_BASE_PATH,/adsp);
static char *GPDSP0_LIBRARY_PATH = combpath(AMFS_BASE_PATH,/gpdsp);
static char *GPDSP1_LIBRARY_PATH = combpath(AMFS_BASE_PATH,/gpdsp1);
static char *CDSP0_LIBRARY_PATH = combpath(AMFS_BASE_PATH,/cdsp);
static char *CDSP1_LIBRARY_PATH = combpath(AMFS_BASE_PATH,/cdsp1);
static char *COMMON_DSP_LIBRARY_PATH = "/usr/lib/rfsa/adsp";
#endif

static QList amfs_std_qlst;
static pthread_mutex_t amfs_std_mt;

int amfs_std_init(void) {
   QList_Ctor(&amfs_std_qlst);
   pthread_mutex_init(&amfs_std_mt, 0);
   return 0;
}

void amfs_std_deinit(void) {
   pthread_mutex_destroy(&amfs_std_mt);
}

static int amfs_FILE_alloc(FILE *stream, amfs_FILE *fd, amfs_FILE hd_base) {
   struct amfs_std_info *sinfo = 0, *info;
   QNode *pn = 0;
   amfs_FILE prevfd = hd_base - 1;
   int nErr = 0;

   AMFS_VERIFY(sinfo = calloc(1, sizeof(*sinfo)), nErr, bail, "Calloc failed");
   QNode_CtorZ(&sinfo->qn);
   sinfo->type = AMFS_STD_STREAM_FILE;
   pthread_mutex_lock(&amfs_std_mt);
   pn = QList_GetFirst(&amfs_std_qlst);
   if(pn) {
      info = STD_RECOVER_REC(struct amfs_std_info, qn, pn);
      prevfd = info->fd;
      QLIST_FOR_REST(&amfs_std_qlst, pn) {
         info = STD_RECOVER_REC(struct amfs_std_info, qn, pn);
         if (info->fd != prevfd + 1) {
            sinfo->fd = prevfd + 1;
            QNode_InsPrev(pn, &sinfo->qn);
            break;
         }
         prevfd = info->fd;
      }
   }
   if(!QNode_IsQueuedZ(&sinfo->qn)) {
      sinfo->fd = prevfd + 1;
      QList_AppendNode(&amfs_std_qlst, &sinfo->qn);
   }
   pthread_mutex_unlock(&amfs_std_mt);

   sinfo->u.stream = stream;
   *fd = sinfo->fd;

   return 0;

bail:
   AMFS_FREEIF(sinfo);
   return nErr;
}

int amfs_FILE_get_std(amfs_FILE fd, struct amfs_std_info** info) {
   struct amfs_std_info *sinfo = NULL;
   QNode *pn, *pnn;
   int nErr = EBADF;

   pthread_mutex_lock(&amfs_std_mt);
   QLIST_NEXTSAFE_FOR_ALL(&amfs_std_qlst, pn, pnn) {
      sinfo = STD_RECOVER_REC(struct amfs_std_info, qn, pn);
      if(sinfo->fd == fd) {
         *info = sinfo;
         nErr = 0;
         break;
      }
   }
   pthread_mutex_unlock(&amfs_std_mt);

   if (nErr)
     AUDIO_BASE_LOG(BASE_LOG_ERROR, MODULE_AMFS, "Can't get file descriptor with given file handle");

   return nErr;
}

static FILE* amfs_FILE_get(amfs_FILE fd) {
    struct amfs_std_info *sinfo = NULL;
    QNode *pn, *pnn;
    FILE* stream = NULL;

    pthread_mutex_lock(&amfs_std_mt);
    QLIST_NEXTSAFE_FOR_ALL(&amfs_std_qlst, pn, pnn) {
        sinfo = STD_RECOVER_REC(struct amfs_std_info, qn, pn);
        if(sinfo->fd == fd) {
            stream = sinfo->u.stream;
            break;
        }
    }
    pthread_mutex_unlock(&amfs_std_mt);
    if (!stream)
        AUDIO_BASE_LOG(BASE_LOG_ERROR, MODULE_AMFS, "Can't get file descriptor with given file handle");

    return stream;
}

int amfs_std_link_modules(void) {
   const char* VENDOR_LIBRARY_PATH = "/vendor/dsp";
   const char* VENDOR_RFSA_LIBRARY_PATH = "/vendor/dsp/rfsa";
   const char* VENDOR_ADSP_LIBRARY_PATH = "/vendor/dsp/rfsa/adsp";
   const char* USR_ADSP_LIBRARY_PATH = "/usr/lib/rfsa/adsp";

   // Check if "/vendor/dsp" path exists
   struct stat st;
   if ((stat(VENDOR_LIBRARY_PATH, &st) != 0) || (S_ISDIR(st.st_mode) == 0)) {
      AUDIO_BASE_LOG(BASE_LOG_ERROR, MODULE_AMFS,
         "amfs_std_link_modules: %s not found", VENDOR_LIBRARY_PATH);
      return -1;
   }

   // Return if modules are already linked
   if ((stat(VENDOR_ADSP_LIBRARY_PATH, &st) == 0) && (S_ISDIR(st.st_mode))) {
      AUDIO_BASE_LOG(BASE_LOG_ERROR, MODULE_AMFS,
         "amfs_std_link_modules: modules already symlinked", VENDOR_LIBRARY_PATH);
      COMMON_DSP_LIBRARY_PATH = VENDOR_ADSP_LIBRARY_PATH;
      return 0;
   }

   // Create the "rfsa" folder
   if ((mkdir(VENDOR_RFSA_LIBRARY_PATH, 0755) != 0)) {
      switch (ERRNO) {
         case EEXIST:
            AUDIO_BASE_LOG(BASE_LOG_INFO, MODULE_AMFS,
               "amfs_std_link_modules: symlinking in existing /vendor/dsp/rfsa");
            break;
         default:
            AUDIO_BASE_LOG(BASE_LOG_INFO, MODULE_AMFS,
               "amfs_std_link_modules: unable to create %s, errno (%s)", VENDOR_RFSA_LIBRARY_PATH, ERRNO);
            return -1;
      }
   }
   // Create a symbolic link to "/usr/lib/rfsa/adsp"
   if (symlink(USR_ADSP_LIBRARY_PATH, VENDOR_ADSP_LIBRARY_PATH) != 0) {
      AUDIO_BASE_LOG(BASE_LOG_ERROR, MODULE_AMFS,
         "amfs_std_link_modules: unable to symlink to %s, errno (%s)", USR_ADSP_LIBRARY_PATH, ERRNO);
      return -1;
   }

   AUDIO_BASE_LOG(BASE_LOG_INFO, MODULE_AMFS,
      "amfs_std_link_modules: symlinked %s to %s successfully",
      USR_ADSP_LIBRARY_PATH, VENDOR_ADSP_LIBRARY_PATH);

   COMMON_DSP_LIBRARY_PATH = VENDOR_ADSP_LIBRARY_PATH;

   return 0;
}

static int amfs_std_getenv(const char* name, char* val, int valLen, int* valLenReq)
{
  const char* vv = getenv(name);
  if(vv) {
    *valLenReq = std_strlen(vv) + 1;
    std_strlcpy(val, vv, valLen);
    return 0;
  }
  AUDIO_BASE_LOG(BASE_LOG_ERROR, MODULE_AMFS, "amfs_std_getenv: %s not found (%s)", name, strerror(ERRNO));
  return ERRNO;
}

int amfs_std_fopen_with_env(const char* envvarname, const char* delim, const char* name,
        const char* mode, amfs_FILE* fh_out, amfs_FILE hd_base)
{
#define     EMTPY_STR      ""
#define     ENV_LEN_GUESS  256
  char     *envList    =  NULL;
  char     *envListBuf =  NULL;
  char     *dirList    =  NULL;
  char     *dirListBuf =  NULL;
  char     *dirName    =  NULL;
  char     *pos        =  NULL;
  char     *absName    =  NULL;
  char     *srcStr     =  NULL;
  uint16   absNameLen  =  0;
  int      nErr        =  0;
  int      envListLen  =  0;
  int      listLen     =  0;
  int      envLenGuess =  0;

  AMFS_VERIFY((NULL != delim) && (NULL != name) && (NULL != mode), nErr, bail, "Invalid input pointer");
  envLenGuess = STD_MAX(ENV_LEN_GUESS,1 + std_strlen(GPDSP1_LIBRARY_PATH) +
                        std_strlen(delim) + std_strlen(COMMON_DSP_LIBRARY_PATH));
  AMFS_VERIFY(envListBuf = (char*)malloc(sizeof(char) * envLenGuess), nErr, bail, "Malloc failed");
  envList = envListBuf;
  *envList = '\0';

  if (0 == amfs_std_getenv(envvarname, envList, envLenGuess, &envListLen)) {
    if (envLenGuess < envListLen) {
      char *tmp = (char*)realloc(envListBuf, sizeof(char) * envListLen);
      AMFS_VERIFY(tmp != NULL, nErr, bail, "Realloc failed");
      envListBuf = tmp;
      envList = envListBuf;
      AMFS_VERIFY(0 == amfs_std_getenv(envvarname, envList, envListLen, &listLen), nErr, bail, "getenv() failed");
    }
  } else {
    if (std_strcmp(envvarname, "ADSP_LIBRARY_PATH") == 0) {
      std_strlcpy(envListBuf, ADSP_LIBRARY_PATH, envLenGuess);
      std_strlcat(envListBuf, delim, envLenGuess);
      envListLen = listLen = 1 + std_strlcat(envListBuf, COMMON_DSP_LIBRARY_PATH, envLenGuess);
    } else if (std_strcmp(envvarname, "GPDSP0_LIBRARY_PATH") == 0) {
      std_strlcpy(envListBuf, GPDSP0_LIBRARY_PATH, envLenGuess);
      std_strlcat(envListBuf, delim, envLenGuess);
      envListLen = listLen = 1 + std_strlcat(envListBuf, COMMON_DSP_LIBRARY_PATH, envLenGuess);
    } else if (std_strcmp(envvarname, "GPDSP1_LIBRARY_PATH") == 0) {
      std_strlcpy(envListBuf, GPDSP1_LIBRARY_PATH, envLenGuess);
      std_strlcat(envListBuf, delim, envLenGuess);
      envListLen = listLen = 1 + std_strlcat(envListBuf, COMMON_DSP_LIBRARY_PATH, envLenGuess);
    }
  }

  /*
   * Allocate mem. to copy envvarname.
   * If envvarname not set, allocate mem to create an empty string
   */
  if('\0' != *envList) {
    srcStr = envList;
  } else {
    srcStr = EMTPY_STR;
    envListLen = std_strlen(EMTPY_STR) + 1;
  }
  AMFS_VERIFY(dirListBuf = (char*)malloc(sizeof(char) * envListLen), nErr, bail, "Malloc failed");
  dirList = dirListBuf;
  std_strlcpy(dirList, srcStr, envListLen);

  while(dirList)
  {
    pos = strstr(dirList, delim);
    dirName = dirList;
    if (pos) {
      *pos = '\0';
      dirList = pos + std_strlen(delim);
    } else {
      dirList = 0;
    }

    /* Account for slash char */
    absNameLen = std_strlen(dirName) + std_strlen(name) + 2;
    AMFS_VERIFY(absName = (char*)malloc(sizeof(char) * absNameLen), nErr, bail, "Malloc failed");
    if ('\0' != *dirName) {
      std_strlcpy(absName, dirName, absNameLen);
      std_strlcat(absName, "/", absNameLen);
      std_strlcat(absName, name, absNameLen);
    } else {
      std_strlcpy(absName, name, absNameLen);
    }

    nErr = amfs_std_fopen(absName, mode, fh_out, hd_base);
    AMFS_FREEIF(absName);
    if (0 == nErr) {
      goto bail;
    }
  }

bail:
  AMFS_FREEIF(absName);
  AMFS_FREEIF(dirListBuf);
  AMFS_FREEIF(envListBuf);

  return nErr;
}

int amfs_std_fopen(const char* name, const char* mode, amfs_FILE* fh_out,
  amfs_FILE hd_base)  {
   FILE* stream = fopen(name, mode);
   int err = 0;

   if(stream) {
      err = amfs_FILE_alloc(stream, fh_out, hd_base);
      AUDIO_BASE_LOG(BASE_LOG_DEBUG, MODULE_AMFS, "amfs_std_fopen: %s, handle: %d", name, *fh_out);
      if (err) {
        AUDIO_BASE_LOG(BASE_LOG_ERROR, MODULE_AMFS, "amfs_std_fopen: File handle alloc failed, close file: %s", name);
        fclose(stream);
      }
      return err;
   }
   AUDIO_BASE_LOG(BASE_LOG_ERROR, MODULE_AMFS, "amfs_std_fopen: failed: %s (Err: %s)", name, strerror(ERRNO));
   return ERRNO;
}

int amfs_std_freopen(amfs_FILE sin, const char* name, const char* mode,
    amfs_FILE* fh_out, amfs_FILE hd_base)  {
   FILE *stream = NULL, *old_stream = NULL;
   int err = 0;

   if (!(old_stream = amfs_FILE_get(sin)))
     return -1;

   stream = freopen(name, mode, old_stream);
   if(stream) {
      err = amfs_FILE_alloc(stream, fh_out, hd_base);
      AUDIO_BASE_LOG(BASE_LOG_DEBUG, MODULE_AMFS, "amfs_std_freopen: %s, old handle: %d, new handle: %d", name, sin, *fh_out);
      if (err) {
        AUDIO_BASE_LOG(BASE_LOG_ERROR, MODULE_AMFS, "amfs_std_freopen: File handle alloc failed, close file: %s", name);
        fclose(stream);
      }
      return err;
   }
   AUDIO_BASE_LOG(BASE_LOG_ERROR, MODULE_AMFS, "amfs_std_freopen: failed: %s (Err: %s)", name, strerror(ERRNO));
   return ERRNO;
}

int amfs_std_fflush(amfs_FILE sin)  {
   FILE *stream = amfs_FILE_get(sin);

   if (!stream)
     return -1;

   AUDIO_BASE_LOG(BASE_LOG_DEBUG, MODULE_AMFS, "amfs_std_fflush: handle: %d", sin);
   if(0 == fflush(stream)) {
      return 0;
   }
   AUDIO_BASE_LOG(BASE_LOG_ERROR, MODULE_AMFS, "amfs_std_fflush: failed (Err: %s)", strerror(ERRNO));
   return ERRNO;
}

int amfs_std_fclose(amfs_FILE sin)  {
   FILE *stream = amfs_FILE_get(sin);
   QNode *qn_ptr = NULL;
   struct amfs_std_info *info = NULL;

   if (!stream)
     return -1;

  QLIST_FOR_ALL(&amfs_std_qlst, qn_ptr) {
     info = STD_RECOVER_REC(struct amfs_std_info, qn, qn_ptr);
     if (info->fd == sin)
     {
       QNode_Dequeue(&info->qn);
       AMFS_FREEIF(info);
       break;
     }
   }

   AUDIO_BASE_LOG(BASE_LOG_DEBUG, MODULE_AMFS, "amfs_std_fclose: handle: %d", sin);
   if(0 == fclose(stream))  {
      return 0;
   }
   AUDIO_BASE_LOG(BASE_LOG_ERROR, MODULE_AMFS, "amfs_std_fclose: failed (Err: %s)", strerror(ERRNO));
   return ERRNO;
}

int amfs_std_fread(amfs_FILE sin, void* buf, int bufLen, int* bytesRead, int* bEOF)  {
   FILE *stream = amfs_FILE_get(sin);
   int out, err = 0;

   if (!stream)
     return -1;

   out = fread(buf, 1, bufLen, stream);
   /* AUDIO_BASE_LOG(BASE_LOG_DEBUG, MODULE_AMFS, "amfs_std_fread: handle: %d, buflen: %d, bytesread: %d",
        sin, bufLen, out); */
   if(0 == out && (0 != (err = ferror(stream)))) {
      return err;
   }
   *bEOF = feof(stream);
   *bytesRead = out;
   return 0;
}

int amfs_std_fwrite(amfs_FILE sin, const void* buf, int bufLen, int* bytesRead,
    int* bEOF) {
   FILE *stream = amfs_FILE_get(sin);
   int out, err = 0;

   if (!stream)
     return -1;

   out = fwrite(buf, 1, bufLen, stream);
   /* AUDIO_BASE_LOG(BASE_LOG_DEBUG, MODULE_AMFS, "amfs_std_fwrite: handle: %d, buflen: %d, byteswrite: %d",
        sin, bufLen, out); */
   *bEOF = 0;
   if(out <= bufLen) {
      if(0 == out && (0 != (err = ferror(stream)))) {
         return err;
      }
      *bEOF = feof(stream);
   }
   *bytesRead = out;
   return 0;
}

int amfs_std_fgetpos(amfs_FILE sin, void* pos, int posLen, int* posLenReq)  {
   fpos_t fpos;
   FILE *stream = amfs_FILE_get(sin);

   if (!stream)
     return -1;

   if(0 == fgetpos(stream, &fpos)) {
      std_memmove(pos, &fpos, STD_MIN((int)sizeof(fpos), posLen));
      *posLenReq = sizeof(fpos);
      return 0;
   }
   return ERRNO;
}
int amfs_std_fsetpos(amfs_FILE sin, const void* pos, int posLen)  {
   FILE *stream = amfs_FILE_get(sin);
   fpos_t fpos;

   if (!stream)
     return -1;

   if(sizeof(fpos) != posLen) {
      return -1;
   }
   std_memmove(&fpos, pos, sizeof(fpos));
   if(0 == fsetpos(stream, &fpos)) {
      return 0;
   }
   return ERRNO;
}
int amfs_std_ftell(amfs_FILE sin, int* pos)  {
   FILE *stream = amfs_FILE_get(sin);

   if (!stream)
     return -1;

   if((*pos = ftell(stream)) >= 0) {
      return 0;
   }
   return ERRNO;
}
int amfs_std_fseek(amfs_FILE sin, int offset, amfs_SEEK whence)  {
   FILE *stream = amfs_FILE_get(sin);

   AMFS_ASSERT(AMFS_SEEK_SET == SEEK_SET);
   AMFS_ASSERT(AMFS_SEEK_CUR == SEEK_CUR);
   AMFS_ASSERT(AMFS_SEEK_END == SEEK_END);

   if (!stream)
     return -1;

   if(0 == fseek(stream, offset, whence)) {
      return 0;
   }
   return ERRNO;
}
int amfs_std_rewind(amfs_FILE sin)  {
   FILE *stream = amfs_FILE_get(sin);

   if (!stream)
     return -1;

   rewind(stream);
   return 0;
}
int amfs_std_feof(amfs_FILE sin, int* bEOF)  {
   FILE *stream = amfs_FILE_get(sin);

   if (!stream)
     return -1;

   *bEOF = feof(stream);
   return 0;
}
int amfs_std_ferror(amfs_FILE sin, int* err)  {
   FILE *stream = amfs_FILE_get(sin);

   if (!stream)
     return -1;

   *err = ferror(stream);
   return 0;
}
int amfs_std_clearerr(amfs_FILE sin)  {
   FILE *stream = amfs_FILE_get(sin);

   if (!stream)
     return -1;

   clearerr(stream);
   return 0;
}
int amfs_std_flen(amfs_FILE sin, uint64_t* len)  {
   struct stat st_buf;
   FILE *stream = amfs_FILE_get(sin);
   int fd;

   if (!stream)
     return -1;

   fd = fileno(stream);
   AMFS_ASSERT(sizeof(st_buf.st_size) <= sizeof(*len));
   if(fd == -1) {
      return ERRNO;
   }
   if(0 != fstat(fd, &st_buf)) {
      return ERRNO;
   }
   *len = st_buf.st_size;
   return 0;
}
