/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef AMFS_CORE_H
#define AMFS_CORE_H

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include <errno.h>
#include "AEEstd.h"
#include "AEEQList.h"
#include "amfs_util.h"
#include "amfs.h"

/* Standard file operations */
#define AMFS_STD_FOPEN      0
#define AMFS_STD_FREOPEN    1
#define AMFS_STD_FFLUSH     2
#define AMFS_STD_FCLOSE     3
#define AMFS_STD_FREAD      4
#define AMFS_STD_FWRITE     5
#define AMFS_STD_FGETPOS    6
#define AMFS_STD_FSETPOS    7
#define AMFS_STD_FTELL      8
#define AMFS_STD_FSEEK      9
#define AMFS_STD_FLEN       10
#define AMFS_STD_REWIND     11
#define AMFS_STD_FEOF       12
#define AMFS_STD_FERROR     13
#define AMFS_STD_CLEARERR   14
#define AMFS_STD_FOPEN_WITH_ENV 15
#define AMFS_PING       16

int amfs_remote_client_invalid(int hab_handle);

#endif //AMFS_CORE_H
