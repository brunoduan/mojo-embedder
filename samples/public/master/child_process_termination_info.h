// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SAMPLES_PUBLIC_MASTER_CHILD_PROCESS_TERMINATION_INFO_H_
#define SAMPLES_PUBLIC_MASTER_CHILD_PROCESS_TERMINATION_INFO_H_

#include "base/process/kill.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "samples/public/common/result_codes.h"

#if defined(OS_ANDROID)
#include "base/android/child_process_binding_types.h"
#endif

namespace samples {

struct ChildProcessTerminationInfo {
  base::TerminationStatus status = base::TERMINATION_STATUS_NORMAL_TERMINATION;

  // If |status| is TERMINATION_STATUS_LAUNCH_FAILED then |exit_code| will
  // contain a platform specific launch failure error code. Otherwise, it will
  // contain the exit code for the process (e.g. status from waitpid if on
  // posix, from GetExitCodeProcess on Windows).
  int exit_code = service_manager::RESULT_CODE_NORMAL_EXIT;

  // Time delta between 1) the process start and 2) the time when
  // ChildProcessTerminationInfo is computed.
  base::TimeDelta uptime = base::TimeDelta::Max();

#if defined(OS_ANDROID)
  // True if child service has strong or moderate binding at time of death.
  base::android::ChildBindingState binding_state =
      base::android::ChildBindingState::UNBOUND;

  // True if child service was explicitly killed by master.
  bool was_killed_intentionally_by_master = false;

  // Counts of remaining child processes with corresponding binding.
  int remaining_process_with_strong_binding = 0;
  int remaining_process_with_moderate_binding = 0;
  int remaining_process_with_waived_binding = 0;
#endif
};

}  // namespace samples

#endif  // SAMPLES_PUBLIC_MASTER_CHILD_PROCESS_TERMINATION_INFO_H_
