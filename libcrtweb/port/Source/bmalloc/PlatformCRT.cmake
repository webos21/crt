# Copyright (c) 2026 The CRT Project.
# SPDX-License-Identifier: Apache-2.0
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# PlatformCRT: B1 reuses the JSCOnly file lists unchanged; the CRT-specific lists start with the
# WebCore milestone. (WEBKIT_INCLUDE_CONFIG_FILES_IF_EXISTS includes Platform${PORT}.cmake.)
include(${CMAKE_CURRENT_LIST_DIR}/PlatformJSCOnly.cmake)

# valgrind.h selects its inline-assembly client request for the x86_64 "Linux" the CRT presents,
# and casts pointers to `unsigned long`, which is 32 bits in the Windows ABI the CRT keeps. There is
# no Valgrind on Windows, so its client requests are compiled out (a documented upstream switch).
if (CMAKE_HOST_WIN32)
    target_compile_definitions(bmalloc PRIVATE NVALGRIND=1)
endif ()
