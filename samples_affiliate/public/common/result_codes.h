// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SAMPLES_PUBLIC_COMMON_RESULT_CODES_H_
#define SAMPLES_PUBLIC_COMMON_RESULT_CODES_H_

#include "services/service_manager/embedder/result_codes.h"

namespace samples {

// This file consolidates all the return codes for the browser and renderer
// process. The return code is the value that:
// a) is returned by main() or winmain(), or
// b) specified in the call for ExitProcess() or TerminateProcess(), or
// c) the exception value that causes a process to terminate.
//
// It is advisable to not use negative numbers because the Windows API returns
// it as an unsigned long and the exception values have high numbers. For
// example EXCEPTION_ACCESS_VIOLATION value is 0xC0000005.
//
// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.samples_public.common
//
// IMPORTANT: This needs to stay in sync with <enum name="CrashExitCodes"> and
// <enum name="WindowsExitCode"> in tools/metrics/histograms/enums.xml. Due to
// chrome::ResultCode's dependency on the enum values, do not add any new values
// here, and mark obsolete values are unused instead of removing them. See
// https://crrev.com/c/885090 for context.
// TODO(wfh): Break the dependency so it is possible to add more values.

enum ResultCode {
  RESULT_CODE_SAMPLES_START = service_manager::RESULT_CODE_LAST_CODE,

  // Process was killed by user or system.
  RESULT_CODE_KILLED = RESULT_CODE_SAMPLES_START,

  // Process hung.
  RESULT_CODE_HUNG,

  // A bad message caused the process termination.
  RESULT_CODE_KILLED_BAD_MESSAGE,

  // The GPU process exited because initialization failed.
  RESULT_CODE_GPU_DEAD_ON_ARRIVAL,

  // Last return code (keep this last).
  RESULT_CODE_LAST_CODE
};

static_assert(RESULT_CODE_LAST_CODE == 5,
              "This enum is frozen - see the IMPORTANT note above.");

}  // namespace samples

#endif  // SAMPLES_PUBLIC_COMMON_RESULT_CODES_H_
