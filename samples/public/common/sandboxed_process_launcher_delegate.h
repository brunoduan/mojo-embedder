// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SAMPLES_PUBLIC_COMMON_SANDBOXED_PROCESS_LAUNCHER_DELEGATE_H_
#define SAMPLES_PUBLIC_COMMON_SANDBOXED_PROCESS_LAUNCHER_DELEGATE_H_

#include "base/environment.h"
#include "base/files/scoped_file.h"
#include "base/process/process.h"
#include "build/build_config.h"
#include "samples/common/export.h"
#include "services/service_manager/sandbox/sandbox_delegate.h"
#include "services/service_manager/sandbox/sandbox_type.h"
#include "services/service_manager/zygote/common/zygote_buildflags.h"

#if BUILDFLAG(USE_ZYGOTE_HANDLE)
#include "services/service_manager/zygote/common/zygote_handle.h"  // nogncheck
#endif  // BUILDFLAG(USE_ZYGOTE_HANDLE)

namespace samples {

// Allows a caller of StartSandboxedProcess or
// MasterChildProcessHost/ChildProcessLauncher to control the sandbox policy,
// i.e. to loosen it if needed.
// The methods below will be called on the PROCESS_LAUNCHER thread.
class SAMPLES_EXPORT SandboxedProcessLauncherDelegate
    : public service_manager::SandboxDelegate {
 public:
  ~SandboxedProcessLauncherDelegate() override {}

#if defined(OS_WIN)
  // SandboxDelegate:
  bool DisableDefaultPolicy() override;
  bool GetAppContainerId(std::string* appcontainer_id) override;
  bool PreSpawnTarget(sandbox::TargetPolicy* policy) override;
  void PostSpawnTarget(base::ProcessHandle process) override;

  // Override to return true if the process should be launched as an elevated
  // process (which implies no sandbox).
  virtual bool ShouldLaunchElevated();
#endif  // defined(OS_WIN)

#if BUILDFLAG(USE_ZYGOTE_HANDLE)
  // Returns the zygote used to launch the process.
  // NOTE: For now Chrome always uses the same zygote for performance reasons.
  // http://crbug.com/569191
  virtual service_manager::ZygoteHandle GetZygote();
#endif  // BUILDFLAG(USE_ZYGOTE_HANDLE)

#if defined(OS_POSIX)
  // Override this if the process needs a non-empty environment map.
  virtual base::EnvironmentMap GetEnvironment();
#endif  // defined(OS_POSIX)
};

}  // namespace samples

#endif  // SAMPLES_PUBLIC_COMMON_SANDBOXED_PROCESS_LAUNCHER_DELEGATE_H_
