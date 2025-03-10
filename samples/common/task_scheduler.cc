// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "samples/common/task_scheduler.h"

#include <algorithm>

#include "base/sys_info.h"

namespace samples {

int GetMinThreadsInSlavererTaskSchedulerForegroundPool() {
  // Assume a busy main thread.
  return std::max(1, base::SysInfo::NumberOfProcessors() - 1);
}

}  // namespace samples
