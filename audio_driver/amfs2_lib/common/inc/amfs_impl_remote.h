/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef AMFS_IMPL_REMOTE_H
#define AMFS_IMPL_REMOTE_H

#define AMFS_HAB_TIMEOUT    0xFFFFFFFF

int amfs_remote_fopen_with_env(int32_t hab_handle, const char* envvarname, const char* delim, const char* name, const char* mode, amfs_FILE* fh_out, amfs_FILE hd_base);
int amfs_remote_fopen(int32_t hab_handle, const char* name, const char* mode, amfs_FILE* fh_out, amfs_FILE hd_base);
int amfs_remote_freopen(int32_t hab_handle, amfs_FILE sin, const char* name, const char* mode,amfs_FILE* fh_out, amfs_FILE hd_base);
int amfs_remote_fgeneral(int32_t hab_handle, amfs_FILE sin, uint32_t opid);
int amfs_remote_fread(int32_t hab_handle, amfs_FILE sin, void *buf, int buflen, int *bytesread, int *beof);
int amfs_remote_fwrite(int32_t hab_handle, amfs_FILE sin, const void *buf, int buflen, int *byteswrite, int *beof);
int amfs_remote_fgetpos(int32_t hab_handle, amfs_FILE sin, void *pos, int poslen, int *poslenreq);
int amfs_remote_fsetpos(int32_t hab_handle, amfs_FILE sin, const void *pos, int poslen);
int amfs_remote_ftell(int32_t hab_handle, amfs_FILE sin, int* pos);
int amfs_remote_fseek(int32_t hab_handle, amfs_FILE sin, int offset, amfs_SEEK whence);
int amfs_remote_flen(int32_t hab_handle, amfs_FILE sin, uint64_t *len);
int amfs_remote_feof(int32_t hab_handle, amfs_FILE sin, int *beof);
int amfs_remote_ferror(int32_t hab_handle, amfs_FILE sin, int *error);
int amfs_remote_ping(int32_t hab_handle);
int amfs_remote_init(void);
void amfs_remote_deinit(void);

#endif /* AMFS_IMPL_REMOTE_H */
