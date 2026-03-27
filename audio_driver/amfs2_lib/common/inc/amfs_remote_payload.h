/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef AMFS_REMOTE_PAYLOAD_H
#define AMFS_REMOTE_PAYLOAD_H

#define AMFS_FILE_NAME_LEN  64
#define AMFS_MODE_LEN   8

struct amfs_fop_header {
  char magic[4];  /* Magic word: AMFS */
  uint32_t op_id; /* File operation ID: AMFS_STD_FOPEN */
};

/* The payload for file open with enviroment operation */
struct amfs_fopen_with_env_req_payload {
  struct amfs_fop_header header;
  int32_t handle_base;
  char env_name[AMFS_FILE_NAME_LEN];
  char delim[AMFS_MODE_LEN];
  char file_name[AMFS_FILE_NAME_LEN];
  char mode[AMFS_MODE_LEN];
};
struct amfs_fopen_with_env_rsp_payload {
  struct amfs_fop_header header;
  int32_t ret_val;
  amfs_FILE file_handle;
};

/* The payload for file open operation */
struct amfs_fopen_req_payload {
  struct amfs_fop_header header;
  int32_t handle_base;
  char file_name[AMFS_FILE_NAME_LEN];
  char mode[AMFS_MODE_LEN];
};
struct amfs_fopen_rsp_payload {
  struct amfs_fop_header header;
  int32_t ret_val;
  amfs_FILE file_handle;
};

/* The payload for file reopen operation */
struct amfs_freopen_req_payload {
  struct amfs_fop_header header;
  amfs_FILE file_handle;
  int32_t handle_base;
  char file_name[AMFS_FILE_NAME_LEN];
  char mode[AMFS_MODE_LEN];
};
struct amfs_freopen_rsp_payload {
  struct amfs_fop_header header;
  int32_t ret_val;
  amfs_FILE file_handle;
};
/* The payload for fflush, fclose, rewind, clearer operations */
struct amfs_fgeneral_req_payload {
  struct amfs_fop_header header;
  amfs_FILE file_handle;
};
struct amfs_fgeneral_rsp_payload {
  struct amfs_fop_header header;
  int32_t ret_val;
};
/* The payload for file read operation */
struct amfs_fread_req_payload {
  struct amfs_fop_header header;
  amfs_FILE file_handle;
  uint32_t buf_size;
};
struct amfs_fread_rsp_payload {
  struct amfs_fop_header header;
  int32_t ret_val;
  uint32_t beof;
  uint32_t read_size;
#if 0
  uint8_t read_buf[buf_size];
#endif
};

/* The payload for file write operation */
struct amfs_fwrite_req_payload {
  struct amfs_fop_header header;
  amfs_FILE file_handle;
  uint32_t buf_size;
#if 0
  uint8_t  buf[buf_size];
#endif
};
struct amfs_fwrite_rsp_payload {
  struct amfs_fop_header header;
  int32_t ret_val;
  uint32_t beof;
  uint32_t written_size;
};

/* The payload for getting file pointer position operation */
struct amfs_fgetpos_req_payload {
  struct amfs_fop_header header;
  amfs_FILE file_handle;
  int32_t poslen;
};
struct amfs_fgetpos_rsp_payload {
  struct amfs_fop_header header;
  int32_t ret_val;
  int32_t poslenreq;
  int32_t fpos_size;
#if 0
  uint8_t fpos[fpos_size];
#endif
};

/* The payload for setting file pointer position operation */
struct amfs_fsetpos_req_payload {
  struct amfs_fop_header header;
  amfs_FILE file_handle;
  int32_t pos_len;
#if 0
  uint8_t pos[pos_len];
#endif
};
struct amfs_fsetpos_rsp_payload {
  struct amfs_fop_header header;
  int32_t ret_val;
};

/* The payload for file tell operation */
struct amfs_ftell_req_payload {
  struct amfs_fop_header header;
  amfs_FILE file_handle;
};
struct amfs_ftell_rsp_payload {
  struct amfs_fop_header header;
  int32_t ret_val;
  uint64_t pos;
};

/* The payload for file seek operation */
struct amfs_fseek_req_payload {
  struct amfs_fop_header header;
  amfs_FILE file_handle;
  int32_t  offset;
  int32_t  whence;
};
struct amfs_fseek_rsp_payload {
  struct amfs_fop_header header;
  int32_t ret_val;
};

/* The payload for file length operation */
struct amfs_flen_req_payload {
  struct amfs_fop_header header;
  amfs_FILE file_handle;
};
struct amfs_flen_rsp_payload {
  struct amfs_fop_header header;
  int32_t ret_val;
  uint64_t len;
};

/* The payload for end of file operation */
struct amfs_feof_req_payload {
  struct amfs_fop_header header;
  amfs_FILE file_handle;
};
struct amfs_feof_rsp_payload {
  struct amfs_fop_header header;
  int32_t ret_val;
  int32_t eof;
};

/* The payload for file error operation */
struct amfs_ferror_req_payload {
  struct amfs_fop_header header;
  amfs_FILE file_handle;
};
struct amfs_ferror_rsp_payload {
  struct amfs_fop_header header;
  int32_t ret_val;
  int32_t error;
};

/* The payload for ping operation */
struct amfs_ping_req_payload {
  struct amfs_fop_header header;
};
struct amfs_ping_rsp_payload {
  struct amfs_fop_header header;
  int32_t ret_val;
};

#endif /* AMFS_REMOTE_PAYLOAD_H */
