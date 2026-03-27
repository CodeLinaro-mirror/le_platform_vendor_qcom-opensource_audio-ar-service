/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef AMFS_IMPL_LOCAL_H
#define AMFS_IMPL_LOCAL_H

int amfs_std_init(void);
void amfs_std_deinit(void);

int amfs_std_fopen_with_env(const char* envvarname, const char* delim, const char* name, const char* mode, amfs_FILE* fh_out, amfs_FILE hd_base);
int amfs_std_fopen(const char* name, const char* mode, amfs_FILE* fh_out, amfs_FILE hd_base);
int amfs_std_freopen(amfs_FILE sin, const char* name, const char* mode, amfs_FILE* fh_out, amfs_FILE hd_base);
int amfs_std_fflush(amfs_FILE sin);
int amfs_std_fclose(amfs_FILE sin);
int amfs_std_fread(amfs_FILE sin, void* buf, int bufLen, int* bytesRead, int* bEOF);
int amfs_std_fwrite(amfs_FILE sin, const void* buf, int bufLen, int* bytesRead, int *bEOF);
int amfs_std_fgetpos(amfs_FILE sin, void* pos, int posLen, int* posLenReq);
int amfs_std_fsetpos(amfs_FILE sin, const void* pos, int posLen);
int amfs_std_ftell(amfs_FILE sin, int* pos);
int amfs_std_fseek(amfs_FILE sin, int offset, amfs_SEEK whence);
int amfs_std_rewind(amfs_FILE sin);
int amfs_std_feof(amfs_FILE sin, int* bEOF);
int amfs_std_ferror(amfs_FILE sin, int* err);
int amfs_std_clearerr(amfs_FILE sin);
int amfs_std_flen(amfs_FILE sin, uint64_t* len);

#endif //AMFS_IMPL_LOCAL_H
