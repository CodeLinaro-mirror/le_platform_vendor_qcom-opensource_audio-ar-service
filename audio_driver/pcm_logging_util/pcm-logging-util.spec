# /*
#  * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
#  * SPDX-License-Identifier: BSD-3-Clause-Clear
#  */

Summary: PCM Logging Utility
Name: pcm-logging-util
Version: 1.0
Release: r0
License: Qualcomm-Technologies-Inc.-Proprietary
URL: http://support.cdmatech.com
Source0: %{name}-%{version}.tar.gz

%define DEPENDS agm
BuildRequires: cmake libtool gcc-g++ agm-devel
Requires: %{DEPENDS}

%description
This is a test application to exercise SPF PCM logging settings via AGM ACDB tunnel APIs.

%package -n pcm-logging-util-devel
Summary: PCM Logging Utility - Development files
License: Qualcomm-Technologies-Inc.-Proprietary
Requires: %{name} = %{version}-%{release}

%description -n pcm-logging-util-devel
This is a test application to exercise SPF PCM logging settings via AGM ACDB tunnel APIs.
This package contains symbolic links, header files
and related items necessary for software development.

%global debug_package %{nil}
%global __os_install_post %{nil}

%prep
%autosetup -n %{name}-%{version}

%build
%cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=$CMAKE_TOOLCHAIN_FILE -DCMAKE_RULE_MESSAGES:BOOL=OFF -DCMAKE_VERBOSE_MAKEFILE:BOOL=$VER -DCMAKE_INSTALL_INCLUDEDIR=%{_includedir} -DCMAKE_INSTALL_LIBDIR=%{_libdir} -DCMAKE_INSTALL_BINDIR=%{_bindir} -DCMAKE_INSTALL_LICENSEDIR=%{_defaultlicensedir}
%cmake_build

%install
%cmake_install

%files
%{_bindir}/pcm_logging_util

%files -n pcm-logging-util-devel
%{_includedir}/pcm_logging_util.h
