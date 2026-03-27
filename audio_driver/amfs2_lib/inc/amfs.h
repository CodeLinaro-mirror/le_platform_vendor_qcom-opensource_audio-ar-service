/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef AMFS_H
#define AMFS_H

#include <stdint.h>
#include "AEEstd.h"
#include "AEEQList.h"

enum amfs_SEEK {
   AMFS_SEEK_SET,
   AMFS_SEEK_CUR,
   AMFS_SEEK_END,
   _32BIT_PLACEHOLDER_amfs_SEEK = 0x7fffffff
};
typedef enum amfs_SEEK amfs_SEEK;
typedef int amfs_FILE;

struct amfs_std_buf_info {
    char* fbuf;
    int flen;
    int pos;
};

struct amfs_std_info {
   QNode qn;
   int type;
   union {
        FILE *stream;
        struct amfs_std_buf_info binfo;
   }u;
   amfs_FILE fd;
};

int amfs_fopen_with_env(const char* envvarname, const char* delim, const char* name, const char* mode, amfs_FILE* fh_out);
int amfs_fopen(const char* name, const char* mode, amfs_FILE* fh_out);
int amfs_freopen(amfs_FILE sin, const char* name, const char* mode, amfs_FILE* fh_out);
int amfs_fflush(amfs_FILE sin);
int amfs_fclose(amfs_FILE sin);
int amfs_fread(amfs_FILE sin, void* buf, int bufLen, int* voidsRead, int* bEOF);
int amfs_fwrite(amfs_FILE sin, const void* buf, int bufLen, int* voidsWritten, int* bEOF);
int amfs_fgetpos(amfs_FILE sin, void* pos, int posLen, int* posLenReq);
int amfs_fsetpos(amfs_FILE sin, const void* pos, int posLen);
int amfs_ftell(amfs_FILE sin, int* pos);
int amfs_fseek(amfs_FILE sin, int offset, amfs_SEEK whence);
int amfs_flen(amfs_FILE sin, uint64_t* len);
int amfs_rewind(amfs_FILE sin);
int amfs_feof(amfs_FILE sin, int* bEOF);
int amfs_ferror(amfs_FILE sin, int* err);
int amfs_clearerr(amfs_FILE sin);

int amfs_init(void);
void amfs_deinit(void);

int amfs_ssr_handling(void);

int amfs_FILE_get_std(amfs_FILE fd, struct amfs_std_info** info);
int amfs_std_link_modules(void);

#endif //AMFS_H
