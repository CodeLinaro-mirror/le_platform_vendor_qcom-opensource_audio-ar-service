#ifndef _ADSP_DEFAULT_LISTENER_H
#define _ADSP_DEFAULT_LISTENER_H

/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef __QAIC_HEADER
#define __QAIC_HEADER(ff) ff
#endif //__QAIC_HEADER

#ifndef __QAIC_HEADER_EXPORT
#define __QAIC_HEADER_EXPORT
#endif // __QAIC_HEADER_EXPORT

#ifndef __QAIC_HEADER_ATTRIBUTE
#define __QAIC_HEADER_ATTRIBUTE
#endif // __QAIC_HEADER_ATTRIBUTE

#ifndef __QAIC_IMPL
#define __QAIC_IMPL(ff) ff
#endif //__QAIC_IMPL

#ifndef __QAIC_IMPL_EXPORT
#define __QAIC_IMPL_EXPORT
#endif // __QAIC_IMPL_EXPORT

#ifndef __QAIC_IMPL_ATTRIBUTE
#define __QAIC_IMPL_ATTRIBUTE
#endif // __QAIC_IMPL_ATTRIBUTE

#ifndef __QAIC_STUB
#define __QAIC_STUB(ff) ff
#endif // __QAIC_STUB

#ifndef __QAIC_STUB_EXPORT
#define __QAIC_STUB_EXPORT
#endif // __QAIC_STUB_EXPORT

#ifndef __QAIC_STUB_ATTRIBUTE
#define __QAIC_STUB_ATTRIBUTE
#endif // __QAIC_STUB_ATTRIBUTE

#ifdef __cplusplus
extern "C" {
#endif
    __QAIC_HEADER_EXPORT void __QAIC_HEADER(adsp_default_listener_deregister)(void) __QAIC_HEADER_ATTRIBUTE;
    __QAIC_HEADER_EXPORT int __QAIC_HEADER(adsp_default_listener_register)(void) __QAIC_HEADER_ATTRIBUTE;
    __QAIC_STUB_EXPORT void __QAIC_STUB(adsp_reset_listener_handle)(void) __QAIC_STUB_ATTRIBUTE;
#ifdef __cplusplus
}
#endif
#endif //_ADSP_DEFAULT_LISTENER_H

