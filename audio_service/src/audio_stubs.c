/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include <stdbool.h>
#include <string.h>
#include "AEEstd.h"
#include "habmm.h"

void* std_memmove(void* pTo, const void* cpFrom, int nLen)
{
   if (nLen <= 0) {
      return pTo;
   }
   return memmove(pTo, cpFrom, (size_t)nLen);
}

int std_strlen(const char* s)
{
   const char *sEnd = s;

   if (!*s)
      return 0;

   while (*++sEnd)
      ;

   return sEnd - s;
}

void* std_memstr(const char* cpHaystack, const char* cpszNeedle,
                 int nHaystackLen)
{
   int nNeedleLen;

   nNeedleLen = std_strlen(cpszNeedle);

   while (nHaystackLen >= nNeedleLen) {
      if (!std_memcmp(cpHaystack, cpszNeedle, nNeedleLen)) {
         return (void*)cpHaystack;
      }
      cpHaystack++;
      nHaystackLen--;
   }
   return 0;

}

char* std_strstr(const char* cpszHaystack, const char* cpszNeedle)
{
   if (0 == std_strlen(cpszNeedle)) { /* strstr(0,"") => 0, shouldn't crash */
      return (char*)cpszHaystack;
   }
   return std_memstr(cpszHaystack, cpszNeedle, std_strlen(cpszHaystack));
}

int std_strlcpy(char* pcDst, const char* cpszSrc, int nDestSize)
{
   int nLen = std_strlen(cpszSrc);

   if (0 < nDestSize) {
      int n;

      n = STD_MIN(nLen, nDestSize - 1);
      (void)std_memmove(pcDst, cpszSrc, n);

      pcDst[n] = 0;
   }

   return nLen;
}

int std_strlcat(char* pcDst, const char* cpszSrc, int nDestSize)
{
   int nLen = 0;

   while ((nLen < nDestSize) && (0 != pcDst[nLen])) {
      ++nLen;
   }

   return nLen + std_strlcpy(pcDst+nLen, cpszSrc, nDestSize-nLen);
}

int std_strncmp(const char* s1, const char* s2, int n)
{
   return strncmp(s1, s2, n);
}

int std_strcmp(const char* s1, const char* s2)
{
   return std_strncmp(s1, s2, MAX_INT32);
}

char* std_strchrend(const char* cpsz, char c)
{
   while (*cpsz && *cpsz != c) {
      ++cpsz;
   }
   return (char*)cpsz;
}

char* std_strchr(const char* cpszSrch, int c)
{
   const char *pc = std_strchrend(cpszSrch, (char)c);

   return (*pc == c ? (char*)pc : 0);
}

int std_memcmp(const void* p1, const void* p2, int length)
{
   const unsigned char *cpc1 = p1;
   const unsigned char *cpc2 = p2;

   while (length-- > 0) {
      int diff = *cpc1++ - *cpc2++;

      if (0 != diff) {
         return diff;
      }
   }
   return 0;
}

#if 0
int32_t habmm_socket_open(int32_t *handle, uint32_t mm_ip_id,
                          uint32_t timeout, uint32_t flags)
{
    return 0;
}

int32_t habmm_socket_close(int32_t handle)
{
    return 0;
}

int32_t habmm_socket_send(int32_t handle, void *src_buff,
                          uint32_t size_bytes, uint32_t flags)
{
    return 0;
}

int32_t habmm_socket_recv(int32_t handle, void *dst_buff, uint32_t *size_bytes,
                          uint32_t timeout, uint32_t flags)
{
    return 0;
}

int32_t habmm_socket_query(int32_t handle, struct hab_socket_info *info,
                           uint32_t flags)
{
    return 0;
}

void msg_send(const msg_const_type * xx_msg_const_ptr)
{
    return;
}

void msg_send_1(const msg_const_type * xx_msg_const_ptr, uint32 xx_arg1)
{
    return;
}

void msg_send_2(const msg_const_type * xx_msg_const_ptr, uint32 xx_arg1, uint32 xx_arg2)
{
    return;
}

void msg_send_3(const msg_const_type * pconst_blk, uint32 xx_arg1, uint32 xx_arg2, uint32 xx_arg3)
{
    return;
}
void msg_v2_send ( const msg_v2_const_type * xx_msg_const_ptr)
{
}

void msg_v2_send_1 (const msg_v2_const_type * xx_msg_const_ptr, uint32 xx_arg1)
{
}

void msg_v2_send_2 ( const msg_v2_const_type * xx_msg_const_ptr,uint32 xx_arg1, uint32 xx_arg2)
{
}

void msg_v2_send_3 ( const msg_v2_const_type * xx_msg_const_ptr, uint32 xx_arg1,
    uint32 xx_arg2, uint32 xx_arg3)
{
}

void msg_v2_send_var ( const msg_v2_const_type * xx_msg_const_ptr, uint32 num_args, ...)
{
}

void msg_v2_errlog_3 (const err_msg_v2_const_type* const_blk, uint32 code1, uint32 code2, uint32 code3)
{
}
void msg_send_var(const msg_const_type * xx_msg_const_ptr, uint32 num_args, ...)
{
    return;
}

void msg_sprintf(const msg_const_type *const_blk, ...)
{
    return;
}
#endif
