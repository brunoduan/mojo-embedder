// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "samples/slaverer/seccomp_sandbox_status_android.h"

#include "samples/public/slaverer/seccomp_sandbox_status_android.h"

namespace samples {

static sandbox::SeccompSandboxStatus g_status =
    sandbox::SeccompSandboxStatus::NOT_SUPPORTED;

void SetSeccompSandboxStatus(sandbox::SeccompSandboxStatus status) {
  g_status = status;
}

sandbox::SeccompSandboxStatus GetSeccompSandboxStatus() {
  return g_status;
}

}  // namespace samples
