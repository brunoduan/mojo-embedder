// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/debug/leak_annotations.h"
#include "base/message_loop/message_loop.h"
#include "base/optional.h"
#include "base/power_monitor/power_monitor.h"
#include "base/run_loop.h"
#include "base/threading/platform_thread.h"
#include "base/timer/hi_res_timer_manager.h"
#include "build/build_config.h"
#include "samples/child/child_process.h"
#include "samples/common/samples_switches_internal.h"
#include "samples/public/common/samples_switches.h"
#include "samples/public/common/main_function_params.h"
#include "samples/public/common/sandbox_init.h"
#include "samples/utility/utility_thread_impl.h"
#include "services/service_manager/sandbox/sandbox.h"

namespace samples {

// Mainline routine for running as the utility process.
int UtilityMain(const MainFunctionParams& parameters) {
  const base::MessageLoop::Type message_loop_type =
      parameters.command_line.HasSwitch(switches::kMessageLoopTypeUi)
          ? base::MessageLoop::TYPE_UI
          : base::MessageLoop::TYPE_DEFAULT;

  // The main message loop of the utility process.
  base::MessageLoop main_message_loop(message_loop_type);
  base::PlatformThread::SetName("CrUtilityMain");

  if (parameters.command_line.HasSwitch(switches::kUtilityStartupDialog))
    WaitForDebugger("Utility");

  ChildProcess utility_process;
  base::RunLoop run_loop;
  utility_process.set_main_thread(
      new UtilityThreadImpl(run_loop.QuitClosure()));

  // Both utility process and service utility process would come
  // here, but the later is launched without connection to service manager, so
  // there has no base::PowerMonitor be created(See ChildThreadImpl::Init()).
  // As base::PowerMonitor is necessary to base::HighResolutionTimerManager, for
  // such case we just disable base::HighResolutionTimerManager for now.
  // Note that disabling base::HighResolutionTimerManager means high resolution
  // timer is always disabled no matter on battery or not, but it should have
  // no any bad influence because currently service utility process is not using
  // any high resolution timer.
  // TODO(leonhsl): Once http://crbug.com/646833 got resolved, re-enable
  // base::HighResolutionTimerManager here for future possible usage of high
  // resolution timer in service utility process.
  base::Optional<base::HighResolutionTimerManager> hi_res_timer_manager;
  if (base::PowerMonitor::Get()) {
    hi_res_timer_manager.emplace();
  }

  run_loop.Run();

#if defined(LEAK_SANITIZER)
  // Invoke LeakSanitizer before shutting down the utility thread, to avoid
  // reporting shutdown-only leaks.
  __lsan_do_leak_check();
#endif

  return 0;
}

}  // namespace samples
