/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include <stdio.h>
#include <string.h>
#include "habmm.h"
#include "amfs_core.h"
#include "amfs_impl_remote.h"
#include "amfs_remote_payload.h"

#define AMFS_HAB_BUFFER_MAX 4096
#define AMFS_FILE_NAME_LEN  64
#define AMFS_MODE_LEN   8

static pthread_mutex_t amfs_remote_mutex;

static inline void amfs_remote_lock(void)
{
  pthread_mutex_lock(&amfs_remote_mutex);
}

static inline void amfs_remote_unlock(void)
{
  pthread_mutex_unlock(&amfs_remote_mutex);
}

int amfs_remote_init(void)
{
  pthread_mutex_init(&amfs_remote_mutex, 0);
  return 0;
}

void amfs_remote_deinit(void)
{
  pthread_mutex_destroy(&amfs_remote_mutex);
}

static int amfs_remote_send(int32_t hab_handle, void *buf_req, uint32_t len_req, void *buf_rsp, uint32_t *len_rsp)
{
  int err = 0;

  AMFS_VERIFY(NULL != buf_req && NULL != buf_rsp && NULL != len_rsp, err, err_exit,
      "Invalid input param");
  /* AUDIO_BASE_LOG(BASE_LOG_INFO, MODULE_AMFS, "amfs_remote_send():hab_handle = 0x%X", hab_handle); */
  memcpy(buf_req, "AMFS", 4);

  amfs_remote_lock();
  err = habmm_socket_send(hab_handle, buf_req, len_req, 0);
  if (err != 0) {
    AUDIO_BASE_LOG(BASE_LOG_ERROR, MODULE_AMFS,"habmm_socket_send() failed(err: %d)", err);
  }
  if (0 == err) {
    while (1) {
      err = habmm_socket_recv(hab_handle, buf_rsp, len_rsp, AMFS_HAB_TIMEOUT, 0);
      if (err == -EAGAIN)
        continue;
      else
        break;
    }
  }
  amfs_remote_unlock();

  if (0 == err) {
    AMFS_VERIFY(0 == strncmp(buf_rsp, "AMFS", 4), err, err_exit, "Invalid magic word");
  }
  if (err != 0) {
    AUDIO_BASE_LOG(BASE_LOG_ERROR, MODULE_AMFS, "HAB comm failed: hab_handle = 0x%X", hab_handle);
    amfs_remote_client_invalid(hab_handle);
  }

err_exit:
  return err;
}

int amfs_remote_fopen_with_env(int32_t hab_handle, const char* envvarname, const char* delim,
        const char* name, const char* mode, amfs_FILE* fh_out, amfs_FILE hd_base)
{
  struct amfs_fopen_with_env_req_payload req;
  struct amfs_fopen_with_env_rsp_payload rsp;
  uint32_t rsp_size = sizeof(rsp);
  int err = 0;

  AMFS_VERIFY(envvarname != NULL && delim != NULL && name != NULL && mode != NULL,
          err, inval_param, "Invalid input parameter");
  AMFS_VERIFY(strlen(envvarname) <= AMFS_FILE_NAME_LEN && strlen(name) <= AMFS_FILE_NAME_LEN,
          err, inval_param, "File name too long");
  AMFS_VERIFY(strlen(delim) <= AMFS_MODE_LEN && strlen(mode) <= AMFS_MODE_LEN,
          err, inval_param, "Mode string too long");

  req.header.op_id = AMFS_STD_FOPEN_WITH_ENV;
  req.handle_base = hd_base;
  memset(req.env_name, '\0', AMFS_FILE_NAME_LEN);
  memset(req.delim, '\0', AMFS_MODE_LEN);
  memset(req.file_name, '\0', AMFS_FILE_NAME_LEN);
  memset(req.mode, '\0', AMFS_MODE_LEN);
  memcpy(req.env_name, envvarname, strlen(envvarname));
  memcpy(req.delim, delim, strlen(delim));
  memcpy(req.file_name, name, strlen(name));
  memcpy(req.mode, mode, strlen(mode));

  err = amfs_remote_send(hab_handle, &req, sizeof(req), &rsp, &rsp_size);
  AMFS_VERIFY(err == 0, err, inval_rsp, "amfs_remote_send() return value is not 0");
  AMFS_VERIFY(rsp_size == sizeof(rsp), err, inval_rsp, "Invalid response message");
  AMFS_VERIFY(rsp.header.op_id == AMFS_STD_FOPEN_WITH_ENV, err, inval_rsp, "Operation ID is not AMFS_STD_FOPEN_WITH_ENV");
  *fh_out = rsp.file_handle;
  err = rsp.ret_val;

inval_rsp:
inval_param:
  return err;
}

int amfs_remote_fopen(int32_t hab_handle, const char* name, const char* mode, amfs_FILE* fh_out, amfs_FILE hd_base)
{
  struct amfs_fopen_req_payload req;
  struct amfs_fopen_rsp_payload rsp;
  uint32_t rsp_size = sizeof(rsp);
  int err = 0;

  AMFS_VERIFY(name != NULL && mode != NULL, err, inval_param, "Invalid input parameter");
  AMFS_VERIFY(strlen(name) <= AMFS_FILE_NAME_LEN, err, inval_param, "File name too long");
  AMFS_VERIFY(strlen(mode) <= AMFS_MODE_LEN, err, inval_param, "Mode string too long");

  AUDIO_BASE_LOG(BASE_LOG_INFO, MODULE_AMFS, "AMFS2 remote fopen");

  amfs_remote_ping(hab_handle);
  req.header.op_id = AMFS_STD_FOPEN;
  req.handle_base = hd_base;
  memset(req.file_name, '\0', AMFS_FILE_NAME_LEN);
  memset(req.mode, '\0', AMFS_MODE_LEN);
  memcpy(req.file_name, name, strlen(name));
  memcpy(req.mode, mode, strlen(mode));

  AUDIO_BASE_LOG(BASE_LOG_INFO, MODULE_AMFS, "AMFS2 remote open send %s %s base %d", req.file_name, req.mode, hd_base);
  err = amfs_remote_send(hab_handle, &req, sizeof(req), &rsp, &rsp_size);
  AMFS_VERIFY(err == 0, err, inval_rsp, "amfs_remote_send() return value is not 0");
  AMFS_VERIFY(rsp_size == sizeof(rsp), err, inval_rsp, "Invalid response message");
  AMFS_VERIFY(rsp.header.op_id == AMFS_STD_FOPEN, err, inval_rsp, "Operation ID is not AMFS_STD_FOPEN");
  *fh_out = rsp.file_handle;
  err = rsp.ret_val;

inval_rsp:
inval_param:
  return err;
}

int amfs_remote_freopen(int32_t hab_handle, amfs_FILE sin, const char* name, const char* mode,amfs_FILE* fh_out, amfs_FILE hd_base)
{
  struct amfs_freopen_req_payload req;
  struct amfs_freopen_rsp_payload rsp;
  uint32_t rsp_size = sizeof(rsp);
  int err = 0;

  AMFS_VERIFY(name != NULL && mode != NULL, err, inval_param, "Invalid input parameter");
  AMFS_VERIFY(strlen(name) <= AMFS_FILE_NAME_LEN, err, inval_param, "File name too long");
  AMFS_VERIFY(strlen(mode) <= AMFS_MODE_LEN, err, inval_param, "Mode string too long");

  req.header.op_id = AMFS_STD_FREOPEN;
  req.file_handle = sin;
  req.handle_base = hd_base;
  memset(req.file_name, '\0', AMFS_FILE_NAME_LEN);
  memset(req.mode, '\0', AMFS_MODE_LEN);
  memcpy(req.file_name, name, strlen(name));
  memcpy(req.mode, mode, strlen(mode));

  err = amfs_remote_send(hab_handle, &req, sizeof(req), &rsp, &rsp_size);
  AMFS_VERIFY(err == 0, err, inval_rsp, "amfs_remote_send() return value is not 0");
  AMFS_VERIFY(rsp_size == sizeof(rsp), err, inval_rsp, "Invalid response message");
  AMFS_VERIFY(rsp.header.op_id == AMFS_STD_FREOPEN, err, inval_rsp, "Operation ID is not AMFS_STD_FREOPEN");
  *fh_out = rsp.file_handle;
  err = rsp.ret_val;

inval_rsp:
inval_param:
  return err;
}

int amfs_remote_fgeneral(int32_t hab_handle, amfs_FILE sin, uint32_t opid)
{
  struct amfs_fgeneral_req_payload req;
  struct amfs_fgeneral_rsp_payload rsp;
  uint32_t rsp_size = sizeof(rsp);
  int err = 0;

  req.header.op_id = opid;
  req.file_handle = sin;

  err = amfs_remote_send(hab_handle, &req, sizeof(req), &rsp, &rsp_size);
  AMFS_VERIFY(err == 0, err, inval_rsp, "amfs_remote_send() return value is not 0");
  AMFS_VERIFY(rsp_size == sizeof(rsp), err, inval_rsp, "Invalid response message");
  AMFS_VERIFY(rsp.header.op_id == opid, err, inval_rsp, "Operation ID is not same as input opid");
  err = rsp.ret_val;

inval_rsp:
  return err;
}

int amfs_remote_fread(int32_t hab_handle, amfs_FILE sin, void *buf, int buflen, int *bytesread, int *beof)
{
  struct amfs_fread_req_payload req;
  struct amfs_fread_rsp_payload *rsp;
  uint32_t rsp_size, rsp_len;
  int err = 0, len_left, sub_len, sub_read_len;
  void *rsp_buf = NULL;
  uint8_t *r_buf = NULL, *ptr = buf;

  AMFS_VERIFY((NULL != buf) && (NULL != bytesread) && (NULL != beof),
      err, inval_param, "Invalid input parameters");
  /* AUDIO_BASE_LOG(BASE_LOG_INFO, MODULE_AMFS, "Reading buf size: %d", buflen); */
  rsp_size = rsp_len = AMFS_HAB_BUFFER_MAX;
  AMFS_VERIFY(rsp_buf = calloc(1, rsp_size), err, calloc_failed, "Calloc rsp buffer failed");

  sub_len = AMFS_HAB_BUFFER_MAX - sizeof(struct amfs_fread_rsp_payload);
  req.header.op_id = AMFS_STD_FREAD;
  req.file_handle = sin;

  r_buf = (uint8_t *)rsp_buf;
  r_buf += sizeof(struct amfs_fread_rsp_payload);
  rsp = (struct amfs_fread_rsp_payload *)rsp_buf;
  len_left = buflen;
  while (len_left > 0) {
    req.buf_size = (sub_len < len_left ? sub_len : len_left);
    err = amfs_remote_send(hab_handle, &req, sizeof(req), rsp_buf, &rsp_size);
    AMFS_VERIFY(err == 0, err, inval_rsp, "amfs_remote_send() return value is not 0");
    AMFS_VERIFY(rsp_size <= rsp_len, err, inval_rsp, "Invalid response message");
    AMFS_VERIFY(rsp->header.op_id == AMFS_STD_FREAD, err, inval_rsp, "Operation ID is not AMFS_STD_FREAD");
    AMFS_VERIFY(rsp->read_size <= rsp_len, err, inval_rsp, "Invalid response size");
    sub_read_len = rsp->read_size;
    len_left -= sub_read_len;
    memcpy(ptr, r_buf, (size_t)sub_read_len);
    ptr += sub_read_len;
    rsp_size = rsp_len;
    if (rsp->beof || rsp->ret_val)
      break;
  }
  *bytesread = buflen - len_left;
  *beof = rsp->beof;
  err = rsp->ret_val;

inval_rsp:
  AMFS_FREEIF(rsp_buf);
calloc_failed:
inval_param:
  return err;
}

int amfs_remote_fwrite(int32_t hab_handle, amfs_FILE sin, const void *buf, int buflen, int *byteswrite, int *beof)
{
  struct amfs_fwrite_req_payload *req;
  struct amfs_fwrite_rsp_payload rsp = {0};
  uint32_t req_size, rsp_size = sizeof(rsp);
  int err = 0, len_left, sub_buflen, sub_reqlen;
  void *req_buf = NULL;
  uint8_t *w_buf = NULL;

  AMFS_VERIFY((NULL != buf) && (NULL != byteswrite) && (NULL != beof),
      err, inval_param, "Invalid input parameters");
  sub_reqlen = AMFS_HAB_BUFFER_MAX - sizeof(struct amfs_fwrite_req_payload);
  AMFS_VERIFY(req_buf = calloc(1, AMFS_HAB_BUFFER_MAX), err, calloc_failed, "Calloc req buffer failed");

  req = (struct amfs_fwrite_req_payload *)req_buf;
  req->header.op_id = AMFS_STD_FWRITE;
  req->file_handle = sin;
  w_buf = (uint8_t *)req_buf;
  w_buf += sizeof(struct amfs_fwrite_req_payload);
  len_left = buflen;

  while(len_left > 0) {
    sub_buflen = (len_left < sub_reqlen ? len_left : sub_reqlen);
    req->buf_size = sub_buflen;
    req_size = sub_buflen + sizeof(struct amfs_fwrite_req_payload);
    memcpy(w_buf, buf, sub_buflen);
    err = amfs_remote_send(hab_handle, req_buf, req_size, &rsp, &rsp_size);
    AMFS_VERIFY(err == 0, err, inval_rsp, "amfs_remote_send() return value is not 0");
    AMFS_VERIFY(rsp_size == sizeof(rsp), err, inval_rsp, "Invalid response message");
    AMFS_VERIFY(rsp.header.op_id == AMFS_STD_FWRITE, err, inval_rsp, "Operation ID is not AMFS_STD_FWRITE");
    AMFS_VERIFY(rsp.written_size <= sub_buflen, err, inval_rsp, "Invalid response size");
    len_left -= rsp.written_size;
    rsp_size = sizeof(rsp);
    if (rsp.beof || rsp.ret_val)
      break;
  }
  *beof = rsp.beof;
  *byteswrite = buflen - len_left;
  err = rsp.ret_val;

inval_rsp:
  AMFS_FREEIF(req_buf);
calloc_failed:
inval_param:
  return err;
}

int amfs_remote_fgetpos(int32_t hab_handle, amfs_FILE sin, void *pos, int poslen, int *poslenreq)
{
  struct amfs_fgetpos_req_payload req;
  struct amfs_fgetpos_rsp_payload *rsp;
  uint32_t rsp_size, rsp_len;
  int err = 0;
  void *rsp_buf = NULL;
  uint8_t *get_buf = NULL;

  AMFS_VERIFY((NULL != pos) && (NULL != poslenreq), err, inval_param, "Invalid input parameters");
  rsp_size = rsp_len = sizeof(struct amfs_fgetpos_rsp_payload) + poslen;
  AMFS_VERIFY(rsp_buf = calloc(1, rsp_size), err, calloc_failed, "Calloc rsp buffer failed");

  req.header.op_id = AMFS_STD_FGETPOS;
  req.file_handle = sin;
  req.poslen = poslen;

  err = amfs_remote_send(hab_handle, &req, sizeof(req), rsp_buf, &rsp_size);
  AMFS_VERIFY(err == 0, err, inval_rsp, "amfs_remote_send() return value is not 0");
  AMFS_VERIFY(rsp_size == rsp_len, err, inval_rsp, "Invalid response message");
  rsp = (struct amfs_fgetpos_rsp_payload *)rsp_buf;
  AMFS_VERIFY(rsp->header.op_id == AMFS_STD_FGETPOS, err, inval_rsp, "Operation ID is not AMFS_STD_FGETPOS");
  *poslenreq = rsp->poslenreq;
  err = rsp->ret_val;
  get_buf = (uint8_t *)rsp_buf;
  get_buf += sizeof(struct amfs_fgetpos_rsp_payload);
  AMFS_VERIFY(rsp->fpos_size <= poslen, err, inval_rsp, "Invalid response length");
  memcpy(pos, get_buf, rsp->fpos_size);

inval_rsp:
  AMFS_FREEIF(rsp_buf);
calloc_failed:
inval_param:
  return err;
}

int amfs_remote_fsetpos(int32_t hab_handle, amfs_FILE sin, const void *pos, int poslen)
{
  struct amfs_fsetpos_req_payload *req;
  struct amfs_fsetpos_rsp_payload rsp;
  uint32_t req_size, rsp_size = sizeof(rsp);
  int err = 0;
  void *req_buf = NULL;
  uint8_t *w_buf = NULL;

  AMFS_VERIFY(NULL != pos, err, inval_param, "Invalid input parameters");
  req_size = sizeof(struct amfs_fsetpos_req_payload) + poslen;
  AMFS_VERIFY(req_buf = calloc(1, req_size), err, calloc_failed, "Calloc req buffer failed");

  req = (struct amfs_fsetpos_req_payload *)req_buf;
  req->header.op_id = AMFS_STD_FSETPOS;
  req->file_handle = sin;
  req->pos_len = poslen;
  w_buf = (uint8_t *)req_buf;
  w_buf += sizeof(struct amfs_fsetpos_req_payload);
  memcpy(w_buf, pos, poslen);

  err = amfs_remote_send(hab_handle, req_buf, req_size, &rsp, &rsp_size);
  AMFS_VERIFY(err == 0, err, inval_rsp, "amfs_remote_send() return value is not 0");
  AMFS_VERIFY(rsp_size == sizeof(rsp), err, inval_rsp, "Invalid response message");
  AMFS_VERIFY(rsp.header.op_id == AMFS_STD_FSETPOS, err, inval_rsp, "Operation ID is not AMFS_STD_FSETPOS");
  err = rsp.ret_val;

inval_rsp:
  AMFS_FREEIF(req_buf);
calloc_failed:
inval_param:
  return err;
}

int amfs_remote_ftell(int32_t hab_handle, amfs_FILE sin, int* pos)
{
  struct amfs_ftell_req_payload req;
  struct amfs_ftell_rsp_payload rsp;
  uint32_t rsp_size = sizeof(rsp);
  int err = 0;

  req.header.op_id = AMFS_STD_FTELL;
  req.file_handle = sin;

  err = amfs_remote_send(hab_handle, &req, sizeof(req), &rsp, &rsp_size);
  AMFS_VERIFY(err == 0, err, inval_rsp, "amfs_remote_send() return value is not 0");
  AMFS_VERIFY(rsp_size == sizeof(rsp), err, inval_rsp, "Invalid response message");
  AMFS_VERIFY(rsp.header.op_id == AMFS_STD_FTELL, err, inval_rsp, "Operation ID is not AMFS_STD_FTELL");
  *pos = rsp.pos;
  err = rsp.ret_val;

inval_rsp:
  return err;
}

int amfs_remote_fseek(int32_t hab_handle, amfs_FILE sin, int offset, amfs_SEEK whence)
{
  struct amfs_fseek_req_payload req;
  struct amfs_fseek_rsp_payload rsp;
  uint32_t rsp_size = sizeof(rsp);
  int err = 0;

  req.header.op_id = AMFS_STD_FSEEK;
  req.file_handle = sin;
  req.offset = offset;
  req.whence = (int32_t)whence;

  err = amfs_remote_send(hab_handle, &req, sizeof(req), &rsp, &rsp_size);
  AMFS_VERIFY(err == 0, err, inval_rsp, "amfs_remote_send() return value is not 0");
  AMFS_VERIFY(rsp_size == sizeof(rsp), err, inval_rsp, "Invalid response message");
  AMFS_VERIFY(rsp.header.op_id == AMFS_STD_FSEEK, err, inval_rsp, "Operation ID is not AMFS_STD_FSEEK");
  err = rsp.ret_val;

inval_rsp:
  return err;
}

int amfs_remote_flen(int32_t hab_handle, amfs_FILE sin, uint64_t *len)
{
  struct amfs_flen_req_payload req;
  struct amfs_flen_rsp_payload rsp;
  uint32_t rsp_size = sizeof(rsp);
  int err = 0;

  req.header.op_id = AMFS_STD_FLEN;
  req.file_handle = sin;

  err = amfs_remote_send(hab_handle, &req, sizeof(req), &rsp, &rsp_size);
  AMFS_VERIFY(err == 0, err, inval_rsp, "amfs_remote_send() return value is not 0");
  AMFS_VERIFY(rsp_size == sizeof(rsp), err, inval_rsp, "Invalid response message");
  AMFS_VERIFY(rsp.header.op_id == AMFS_STD_FLEN, err, inval_rsp, "Operation ID is not AMFS_STD_FLEN");
  *len = rsp.len;
  err = rsp.ret_val;

inval_rsp:
  return err;
}

int amfs_remote_feof(int32_t hab_handle, amfs_FILE sin, int *beof)
{
  struct amfs_feof_req_payload req;
  struct amfs_feof_rsp_payload rsp;
  uint32_t rsp_size = sizeof(rsp);
  int err = 0;

  req.header.op_id = AMFS_STD_FEOF;
  req.file_handle = sin;

  err = amfs_remote_send(hab_handle, &req, sizeof(req), &rsp, &rsp_size);
  AMFS_VERIFY(err == 0, err, inval_rsp, "amfs_remote_send() return value is not 0");
  AMFS_VERIFY(rsp_size == sizeof(rsp), err, inval_rsp, "Invalid response message");
  AMFS_VERIFY(rsp.header.op_id == AMFS_STD_FEOF, err, inval_rsp, "Operation ID is not AMFS_STD_FEOF");
  *beof = rsp.eof;
  err = rsp.ret_val;

inval_rsp:
  return err;
}

int amfs_remote_ferror(int32_t hab_handle, amfs_FILE sin, int *error)
{
  struct amfs_ferror_req_payload req;
  struct amfs_ferror_rsp_payload rsp;
  uint32_t rsp_size = sizeof(rsp);
  int err = 0;

  req.header.op_id = AMFS_STD_FERROR;
  req.file_handle = sin;

  err = amfs_remote_send(hab_handle, &req, sizeof(req), &rsp, &rsp_size);
  AMFS_VERIFY(err == 0, err, inval_rsp, "amfs_remote_send() return value is not 0");
  AMFS_VERIFY(rsp_size == sizeof(rsp), err, inval_rsp, "Invalid response message");
  AMFS_VERIFY(rsp.header.op_id == AMFS_STD_FERROR, err, inval_rsp, "Operation ID is not AMFS_STD_FERROR");
  *error = rsp.error;
  err = rsp.ret_val;

inval_rsp:
  return err;
}

/* This message is to check if registered client alive/avaliable,
 * if not, deregister it. So that we can handle the situation that
 * GVM retart (AMFS front end on GVM restart).
 */
int amfs_remote_ping(int32_t hab_handle)
{
  struct amfs_ferror_req_payload req;
  struct amfs_ferror_rsp_payload rsp;
  uint32_t rsp_size = sizeof(rsp);
  int err = 0;

  req.header.op_id = AMFS_PING;

  err = amfs_remote_send(hab_handle, &req, sizeof(req), &rsp, &rsp_size);
  AMFS_VERIFY(err == 0, err, inval_hab, "amfs_remote_send() return value is not 0");
  AMFS_VERIFY(rsp_size == sizeof(rsp), err, inval_rsp, "Invalid response message");
  AMFS_VERIFY(rsp.header.op_id == AMFS_PING, err, inval_rsp, "Operation ID is not AMFS_STD_PING");
  AMFS_VERIFY(0  == rsp.ret_val, err, inval_rsp, "Ping: ret_val is not zero");
  AUDIO_BASE_LOG(BASE_LOG_INFO, MODULE_AMFS, "Ping success,  hab_handle = 0x%X", hab_handle);
  return 0;

inval_rsp:
  amfs_remote_client_invalid(hab_handle);
inval_hab:
  AUDIO_BASE_LOG(BASE_LOG_INFO, MODULE_AMFS, "Ping failed, deregistration done, hab_handle = 0x%X", hab_handle);
  return err;
}
