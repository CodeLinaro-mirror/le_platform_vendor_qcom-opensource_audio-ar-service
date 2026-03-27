/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#define _GNU_SOURCE
#include "habmm.h"
#include "amfs_core.h"
#include "amfs_impl_std.h"
#include "amfs_impl_remote.h"
#include "amfs_remote_payload.h"
#ifdef AUDIO_TARGET_VMM_SUPPORTED
  #include "avmm.h"
#endif
#if defined(USE_GLIB)
#include <glib.h>
#define strlcpy g_strlcpy
#endif

#define AMFS_VM_NUM_MAX       32    /* Support upto 32 VMs */
#define AMFS_HANDLE_BASE_STEP 1000  /* Support 1000 modules on each VM */
#define AMFS_HAB_MINOR_ID     8
#define AMFS_PREFIX_LEN       8
#define AMFS_PREFIX_BUF_LEN   9
#define AMFS_PREFIX_VM_LEN    4
#define AMFS_PREFIX_VMID_LEN  3
#define AMFS_PREFIX_VM_STR    "/vm/"
#define AMFS_PREFIX_FORMAT    "/vm/%03d/" /* The length of prefix should be multiple of 4 */

struct amfs_vm_client {
  QNode    qn;
  int32_t  client_id;
  int32_t  hab_handle;
  bool     local_flag; /* true: PVM, false: GVM */
  int      handle_base;  /* Cardinal number of file handle */
  int32_t  vmid;
#ifdef AUDIO_TARGET_VMM_SUPPORTED
  struct avmm_connection *avmm_connection;
#endif
};

static QList amfs_client_list;
static pthread_mutex_t amfs_client_mutex;
static pthread_t amfs_hab_thread;
static bool amfs_monitor_exit_flag;
#ifdef AUDIO_TARGET_VMM_SUPPORTED
  static struct avmm_client *amfs_avmm_client = NULL;
#endif

static inline void amfs_client_lock(void)
{
  pthread_mutex_lock(&amfs_client_mutex);
}

static inline void amfs_client_unlock(void)
{
  pthread_mutex_unlock(&amfs_client_mutex);
}

static int amfs_add_client(struct amfs_vm_client **client_vm)
{
  struct amfs_vm_client *client = NULL, *sclient;
  int32_t prev_base;
  QNode *qn_ptr = NULL;
  int err = 0;

  AMFS_VERIFY(client = calloc(1, sizeof(struct amfs_vm_client)), err, malloc_fail,
    "struct amfs_vm_client malloc failed");
  QNode_CtorZ(&client->qn);
  amfs_client_lock();
  qn_ptr = QList_GetFirst(&amfs_client_list);
  if (qn_ptr) {
    sclient = STD_RECOVER_REC(struct amfs_vm_client, qn, qn_ptr);
    prev_base = sclient->handle_base;
    QLIST_FOR_REST(&amfs_client_list, qn_ptr) {
      sclient = STD_RECOVER_REC(struct amfs_vm_client, qn, qn_ptr);
      if (sclient->handle_base == prev_base + AMFS_HANDLE_BASE_STEP) {
        prev_base = sclient->handle_base;
        continue;
      }
      client->handle_base = prev_base + AMFS_HANDLE_BASE_STEP;
      AMFS_VERIFY(client->handle_base < AMFS_HANDLE_BASE_STEP * AMFS_VM_NUM_MAX,
        err, handle_exeed, "The number of VM (%d) has exceeded maximum(%d)",
        client->handle_base / AMFS_HANDLE_BASE_STEP, AMFS_HANDLE_BASE_STEP);
      QNode_InsPrev(qn_ptr, &client->qn);
      break;
    }
  } else {
    client->handle_base = 0;  /* Handle base start from 0 */
    QList_AppendNode(&amfs_client_list, &client->qn);
  }
  client->client_id = client->handle_base / AMFS_HANDLE_BASE_STEP;
  *client_vm = client;
  amfs_client_unlock();
  return 0;

handle_exeed:
  amfs_client_unlock();
malloc_fail:
  AMFS_FREEIF(client);
  return err;
}

static int amfs_add_local_client(struct amfs_vm_client **client_vm)
{
  struct amfs_vm_client *client;
  int err = 0;

  if(!(err = amfs_add_client(&client))) {
    client->local_flag = true;
    *client_vm = client;
    AUDIO_BASE_LOG(BASE_LOG_DEBUG, MODULE_AMFS, "Local client added");
  }
  return err;
}

static int amfs_add_remote_client(int32_t handle, struct amfs_vm_client **client_vm)
{
  struct amfs_vm_client *client;
  struct amfs_vm_client *cptr = NULL;
  QNode *qn_ptr = NULL;
  int err = 0;
  struct hab_socket_info socket_info;

  amfs_client_lock();
  QLIST_FOR_ALL(&amfs_client_list, qn_ptr) {
    cptr = STD_RECOVER_REC(struct amfs_vm_client, qn, qn_ptr);
    if (!cptr->local_flag) {/* For GVMs */
      err = amfs_remote_ping(cptr->hab_handle);
      if (err)
        break;
    }
  }
  amfs_client_unlock();

  AMFS_VERIFY(0 == amfs_add_client(&client), err, bail, "Add remote client failed");
  client->local_flag = false;
  client->hab_handle = handle;
  *client_vm = client;
  AUDIO_BASE_LOG(BASE_LOG_DEBUG, MODULE_AMFS, "Remote client added, client_id = 0x%X, hab_handle = 0x%X", client->client_id, handle);

  AMFS_VERIFY(0 == habmm_socket_query(handle, &socket_info, 0),
                   err, bail, "habmm_socket_query failed");

  client->vmid = socket_info.vmid_remote;

#ifdef AUDIO_TARGET_VMM_SUPPORTED
  client->avmm_connection = avmm_notify_connect(amfs_avmm_client,
                                                socket_info.vmid_remote,
                                                (int32_t *)(ssize_t)client->client_id);
  AMFS_VERIFY(NULL != client->avmm_connection, err, bail, "avmm_notify_connect failed");
#endif

bail:
  return err;
}

static int amfs_remove_client(struct amfs_vm_client *client)
{
  int err = 0;

  AMFS_VERIFY(NULL != client, err, unavail_ptr, "Unavailable client to be removed");
  if (!client->local_flag)
    habmm_socket_close(client->hab_handle);
  QNode_Dequeue(&client->qn);
  AMFS_FREEIF(client);

unavail_ptr:
  return err;
}

static void amfs_cleanup(void* client)
{
  struct amfs_vm_client *cptr = NULL, *client_vm = NULL;
  QNode *qn_ptr = NULL;
  int err = 0;
  int32_t client_id = (int32_t)(uintptr_t)client;

  amfs_client_lock();
  QLIST_FOR_ALL(&amfs_client_list, qn_ptr) {
    cptr = STD_RECOVER_REC(struct amfs_vm_client, qn, qn_ptr);
    if (client_id == cptr->client_id) {
      client_vm = cptr;
      break;
    }
  }

  if (client_vm) {
    err = amfs_remove_client(client_vm);
  } else {
    AUDIO_BASE_LOG(BASE_LOG_ERROR, MODULE_AMFS, "Can't find client with this client_id(%d)", client_id);
    err = -1;
  }
  if(err)
    AUDIO_BASE_LOG(BASE_LOG_ERROR, MODULE_AMFS, "%s Cleanup error", __func__);
  amfs_client_unlock();

  return ;
}

static int amfs_get_client_with_handle(struct amfs_vm_client **client, int handle)
{
  struct amfs_vm_client *cptr = NULL, *clt = NULL;
  QNode *qn_ptr = NULL;
  int err = 0;

  amfs_client_lock();
  QLIST_FOR_ALL(&amfs_client_list, qn_ptr) {
    cptr = STD_RECOVER_REC(struct amfs_vm_client, qn, qn_ptr);
    if (handle >= cptr->handle_base &&
        handle < cptr->handle_base + AMFS_HANDLE_BASE_STEP) {
      clt = cptr;
      *client = cptr;
      break;
    }
  }
  amfs_client_unlock();
  if (!clt) {
    AUDIO_BASE_LOG(BASE_LOG_ERROR, MODULE_AMFS, "Can't find client with this file handle(%d)", handle);
    err = -1;
  }

  return err;
}

static void* amfs_hab_monitor(void* arg) {
  struct amfs_vm_client *client_vm = NULL;
  int32_t hab_handle;
  int err = 0;

  AUDIO_BASE_LOG(BASE_LOG_DEBUG, MODULE_AMFS, "HAB channel monitor thread started");
  pthread_setname_np(pthread_self(), "AMFS_HAB_MONITOR");

  while(!amfs_monitor_exit_flag) {
    err = habmm_socket_open(&hab_handle,
      HAB_MMID_CREATE( MM_AUD_4, AMFS_HAB_MINOR_ID),
      AMFS_HAB_TIMEOUT,
      HABMM_SOCKET_OPEN_FLAGS_SINGLE_BE_SINGLE_FE);
    if (err == -EAGAIN)
      continue;
    if (err != 0) {
      AUDIO_BASE_LOG(BASE_LOG_ERROR, MODULE_AMFS, "Open HAB socket failed (err = %d)", err);
      break;
    }
    AUDIO_BASE_LOG(BASE_LOG_DEBUG, MODULE_AMFS, "Open HAB socket success, handle: 0x%X", hab_handle);

    err = amfs_add_remote_client(hab_handle, &client_vm);
    if (err) {
      AUDIO_BASE_LOG(BASE_LOG_ERROR, MODULE_AMFS, "Add remote client with handle(0x%X) failed", hab_handle);
      continue;
    }
  } /* while() */
  AUDIO_BASE_LOG(BASE_LOG_DEBUG, MODULE_AMFS, "HAB channel monitor thread exit(err: %d)", err);
  return NULL;
}

int amfs_remote_client_invalid(int32_t hab_handle)
{
  struct amfs_vm_client *cptr = NULL, *client_inval = NULL;
  QNode *qn_ptr = NULL;
  int err = 0;

  amfs_client_lock();
  QLIST_FOR_ALL(&amfs_client_list, qn_ptr) {
    cptr = STD_RECOVER_REC(struct amfs_vm_client, qn, qn_ptr);
    if (hab_handle == cptr->hab_handle) {
      client_inval = cptr;
      break;
    }
  }

  if (client_inval) {
    err = amfs_remove_client(client_inval);
  } else {
    AUDIO_BASE_LOG(BASE_LOG_ERROR, MODULE_AMFS, "Can't find client with this HAB handle(0x%X)", hab_handle);
    err = -1;
  }
  amfs_client_unlock();

  return err;
}

int amfs_init(void)
{
  struct amfs_vm_client *client_vm;
  int err = 0;

  QList_Ctor(&amfs_client_list);
  amfs_remote_init();
  pthread_mutex_init(&amfs_client_mutex, 0);
  amfs_monitor_exit_flag = false;
  AMFS_VERIFY(0 == amfs_add_local_client(&client_vm), err, init_failed,
      "Add local client failed");
  AMFS_VERIFY(0 == amfs_std_init(), err, init_failed,
      "Init local client failed");
  AMFS_VERIFY(0 == amfs_std_link_modules(), err, init_failed,
      "Link audio modules failed");

#ifdef AUDIO_TARGET_VMM_SUPPORTED
  amfs_avmm_client = avmm_create_client(avmm_get_avmm(),
                                        "AMFS",
                                        amfs_cleanup);
  AMFS_VERIFY(NULL != amfs_avmm_client, err, init_failed, "Create AVMM client failed");
#endif

#if 0
  AMFS_VERIFY(0 == pthread_create(&amfs_hab_thread, NULL, amfs_hab_monitor, NULL),
      err, init_failed, "Create HAB monitor thread failed(err: %d)", err);
#endif

  AUDIO_BASE_LOG(BASE_LOG_DEBUG, MODULE_AMFS, "Init success");
  return 0;

init_failed:
#ifdef AUDIO_TARGET_VMM_SUPPORTED
  avmm_destroy_client(amfs_avmm_client);
#endif
  AUDIO_BASE_LOG(BASE_LOG_ERROR, MODULE_AMFS, "Init failed");
  return err;
}

void amfs_deinit(void) {
  struct amfs_vm_client *cptr = NULL;
  QNode *qn_ptr = NULL;
  void *exit;

  amfs_monitor_exit_flag = true;
  pthread_join(amfs_hab_thread, &exit);
#ifdef AUDIO_TARGET_VMM_SUPPORTED
  avmm_destroy_client(amfs_avmm_client);
#endif
  amfs_std_deinit();
  amfs_client_lock();
  QLIST_FOR_ALL(&amfs_client_list, qn_ptr) {
    cptr = STD_RECOVER_REC(struct amfs_vm_client, qn, qn_ptr);
    amfs_remove_client(cptr);
  }
  amfs_client_unlock();
  pthread_mutex_destroy(&amfs_client_mutex);
  amfs_remote_deinit();
  AUDIO_BASE_LOG(BASE_LOG_DEBUG, MODULE_AMFS,"Deinit done");
}

/* ========================= AMFS file operations ============================= */

int amfs_fopen_with_env(const char* envvarname, const char* delim, const char* name,
        const char* mode, amfs_FILE* fh_out)
{
  struct amfs_vm_client *cptr = NULL;
  QNode *qn_ptr = NULL;
  bool vm_found = false;
  const char *file_name = NULL;
  char client_id[AMFS_PREFIX_VM_LEN] = {0};
  int vmid, err = 0;

  AMFS_VERIFY(name != NULL, err, bail, "Invalid file name");
  AUDIO_BASE_LOG(BASE_LOG_DEBUG, MODULE_AMFS, "AMFS2 Opening file: %s", name);

  //TBD: if we would like to load .so module in GVM, then GSL need parse AMDB and append vm prefix to module name
  /* Parse VM prefix: /vm/xxx/ */
  if (0 == strncmp(name, AMFS_PREFIX_VM_STR, AMFS_PREFIX_VM_LEN)) { /* For GVM */
    strlcpy(client_id, name + AMFS_PREFIX_VM_LEN, AMFS_PREFIX_VMID_LEN + 1); /*For Termination*/
    vmid = atoi(client_id);
    file_name = name + AMFS_PREFIX_LEN - 1; /*we have to keep / for the acdb absolute path*/
  } else { /* For PVM */
    vmid = 0;
    file_name = name;
  }

  amfs_client_lock();
  QLIST_FOR_ALL(&amfs_client_list, qn_ptr) {
    cptr = STD_RECOVER_REC(struct amfs_vm_client, qn, qn_ptr);
    //if (cptr->client_id == vmid) {
    if (cptr && (cptr->vmid == vmid)) {
      vm_found = true;
      break;
    }
  }
  amfs_client_unlock();

  if (cptr == NULL) {
    AUDIO_BASE_LOG(BASE_LOG_ERROR, MODULE_AMFS, "AMFS2 Opening file name wit prefix: %s get handle failed", file_name);
    goto bail;
  } else {
    AUDIO_BASE_LOG(BASE_LOG_DEBUG, MODULE_AMFS, "AMFS2 Opening file name wit prefix: %s handle base %d", file_name, cptr->handle_base);
  }

  AMFS_VERIFY(true == vm_found, err, bail, "VM client can't found with client_id %d", vmid);
  if (cptr->local_flag) /* For PVM */
    if (NULL == envvarname)
      err = amfs_std_fopen(name, mode, fh_out, cptr->handle_base);
    else
      err = amfs_std_fopen_with_env(envvarname, delim, file_name, mode, fh_out, cptr->handle_base);
  else /* For GVMs */
    if (NULL == envvarname)
      err = amfs_remote_fopen(cptr->hab_handle, file_name, mode, fh_out, cptr->handle_base);
    else
      err = amfs_remote_fopen_with_env(cptr->hab_handle, envvarname, delim, file_name, mode,
            fh_out, cptr->handle_base);
  if (!err)
    AUDIO_BASE_LOG(BASE_LOG_DEBUG, MODULE_AMFS, "File %s is opened success on VM %d", file_name, cptr->client_id);
  else
    AUDIO_BASE_LOG(BASE_LOG_ERROR, MODULE_AMFS, "File %s is opened fail on VM %d, err = %d", file_name, cptr->client_id, err);

bail:
  return err;
}

int amfs_fopen(const char* name, const char* mode, amfs_FILE* fh_out)
{

    AUDIO_BASE_LOG(BASE_LOG_DEBUG, MODULE_AMFS, "amfs fopen in");
  return amfs_fopen_with_env(NULL, NULL, name, mode, fh_out);
}

int amfs_freopen(amfs_FILE sin, const char* name, const char* mode, amfs_FILE* fh_out)
{
  struct amfs_vm_client *cptr = NULL;
  int err = 0;

  err = amfs_get_client_with_handle(&cptr, sin);
  if (!err) {
    if (cptr->local_flag) /* For PVM */
      err = amfs_std_freopen(sin, name, mode, fh_out, cptr->handle_base);
    else /* For GVMs */
      err = amfs_remote_freopen(cptr->hab_handle, sin, name, mode, fh_out, cptr->handle_base);
  }

  return err;
}
int amfs_fflush(amfs_FILE sin)
{
  struct amfs_vm_client *cptr = NULL;
  int err = 0;

  err = amfs_get_client_with_handle(&cptr, sin);
  if (!err) {
    if (cptr->local_flag) /* For PVM */
      err = amfs_std_fflush(sin);
    else /* For GVMs */
      err = amfs_remote_fgeneral(cptr->hab_handle, sin, AMFS_STD_FFLUSH);
  }

  return err;
}
int amfs_fclose(amfs_FILE sin)
{
  struct amfs_vm_client *cptr = NULL;
  int err = 0;

  err = amfs_get_client_with_handle(&cptr, sin);
  if (!err) {
    if (cptr->local_flag) /* For PVM */
      err = amfs_std_fclose(sin);
    else /* For GVMs */
      err = amfs_remote_fgeneral(cptr->hab_handle, sin, AMFS_STD_FCLOSE);
  }

  return err;
}

/* If we are asked to read a size that over the maximum size that HAB can
* transfer. We need chop the callings to multi times */
int amfs_fread(amfs_FILE sin, void* buf, int bufLen, int* voidsRead, int* bEOF)
{
  struct amfs_vm_client *cptr = NULL;
  int err = 0;

  err = amfs_get_client_with_handle(&cptr, sin);
  if (!err) {
    if (cptr->local_flag) /* For PVM */
      err = amfs_std_fread(sin, buf, bufLen, voidsRead, bEOF);
    else /* For GVMs */
      err = amfs_remote_fread(cptr->hab_handle, sin, buf, bufLen, voidsRead, bEOF);
  }

  return err;
}
int amfs_fwrite(amfs_FILE sin, const void* buf, int bufLen, int* voidsWritten, int* bEOF)
{
  struct amfs_vm_client *cptr = NULL;
  int err = 0;

  err = amfs_get_client_with_handle(&cptr, sin);
  if (!err) {
    if (cptr->local_flag) /* For PVM */
      err = amfs_std_fwrite(sin, buf, bufLen, voidsWritten, bEOF);
    else /* For GVMs */
      err = amfs_remote_fwrite(cptr->hab_handle, sin, buf, bufLen, voidsWritten, bEOF);
  }

  return err;
}
int amfs_fgetpos(amfs_FILE sin, void* pos, int posLen, int* posLenReq)
{
  struct amfs_vm_client *cptr = NULL;
  int err = 0;

  err = amfs_get_client_with_handle(&cptr, sin);
  if (!err) {
    if (cptr->local_flag) /* For PVM */
      err = amfs_std_fgetpos(sin, pos, posLen, posLenReq);
    else /* For GVMs */
      err = amfs_remote_fgetpos(cptr->hab_handle, sin, pos, posLen, posLenReq);
  }

  return err;
}
int amfs_fsetpos(amfs_FILE sin, const void* pos, int posLen)
{
  struct amfs_vm_client *cptr = NULL;
  int err = 0;

  err = amfs_get_client_with_handle(&cptr, sin);
  if (!err) {
    if (cptr->local_flag) /* For PVM */
      err = amfs_std_fsetpos(sin, pos, posLen);
    else /* For GVMs */
      err = amfs_remote_fsetpos(cptr->hab_handle, sin, pos, posLen);
  }

  return err;
}
int amfs_ftell(amfs_FILE sin, int* pos)
{
  struct amfs_vm_client *cptr = NULL;
  int err = 0;

  err = amfs_get_client_with_handle(&cptr, sin);
  if (!err) {
    if (cptr->local_flag) /* For PVM */
      err = amfs_std_ftell(sin, pos);
    else /* For GVMs */
      err = amfs_remote_ftell(cptr->hab_handle, sin, pos);
  }

  return err;
}
int amfs_fseek(amfs_FILE sin, int offset, amfs_SEEK whence)
{
  struct amfs_vm_client *cptr = NULL;
  int err = 0;

  err = amfs_get_client_with_handle(&cptr, sin);
  if (!err) {
    if (cptr->local_flag) /* For PVM */
      err = amfs_std_fseek(sin, offset, whence);
    else /* For GVMs */
      err = amfs_remote_fseek(cptr->hab_handle, sin, offset, whence);
  }

  return err;
}
int amfs_flen(amfs_FILE sin, uint64_t* len)
{
  struct amfs_vm_client *cptr = NULL;
  int err = 0;

  err = amfs_get_client_with_handle(&cptr, sin);
  if (!err) {
    if (cptr->local_flag) /* For PVM */
      err = amfs_std_flen(sin, len);
    else /* For GVMs */
      err = amfs_remote_flen(cptr->hab_handle, sin, len);
  }

  return err;
}
int amfs_rewind(amfs_FILE sin)
{
  struct amfs_vm_client *cptr = NULL;
  int err = 0;

  err = amfs_get_client_with_handle(&cptr, sin);
  if (!err) {
    if (cptr->local_flag) /* For PVM */
      err = amfs_std_rewind(sin);
    else /* For GVMs */
      err = amfs_remote_fgeneral(cptr->hab_handle, sin, AMFS_STD_REWIND);
  }

  return err;
}
int amfs_feof(amfs_FILE sin, int* bEOF)
{
  struct amfs_vm_client *cptr = NULL;
  int err = 0;

  err = amfs_get_client_with_handle(&cptr, sin);
  if (!err) {
    if (cptr->local_flag) /* For PVM */
      err = amfs_std_feof(sin, bEOF);
    else /* For GVMs */
      err = amfs_remote_feof(cptr->hab_handle, sin, bEOF);
  }

  return err;
}
int amfs_ferror(amfs_FILE sin, int* error)
{
  struct amfs_vm_client *cptr = NULL;
  int err = 0;

  err = amfs_get_client_with_handle(&cptr, sin);
  if (!err && error) {
    if (cptr->local_flag) /* For PVM */
      err = amfs_std_ferror(sin, error);
    else /* For GVMs */
      err = amfs_remote_ferror(cptr->hab_handle, sin, error);
  }

  return err;
}
int amfs_clearerr(amfs_FILE sin)
{
  struct amfs_vm_client *cptr = NULL;
  int err = 0;

  err = amfs_get_client_with_handle(&cptr, sin);
  if (!err) {
    if (cptr->local_flag) /* For PVM */
      err = amfs_std_clearerr(sin);
    else /* For GVMs */
      err = amfs_remote_fgeneral(cptr->hab_handle, sin, AMFS_STD_CLEARERR);
  }

  return err;
}
