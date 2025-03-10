// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "samples/utility/in_process_utility_thread.h"

#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "samples/child/child_process.h"
#include "samples/utility/utility_thread_impl.h"

namespace samples {

// We want to ensure there's only one utility thread running at a time, as there
// are many globals used in the utility process.
static base::LazyInstance<base::Lock>::DestructorAtExit
    g_one_utility_thread_lock;

InProcessUtilityThread::InProcessUtilityThread(
    const InProcessChildThreadParams& params)
    : Thread("Samples_InProcUtilityThread"), params_(params) {
}

InProcessUtilityThread::~InProcessUtilityThread() {
  // Wait till in-process utility thread finishes clean up.
  bool previous_value = base::ThreadRestrictions::SetIOAllowed(true);
  Stop();
  base::ThreadRestrictions::SetIOAllowed(previous_value);
}

void InProcessUtilityThread::Init() {
  // We need to return right away or else the main thread that started us will
  // hang.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&InProcessUtilityThread::InitInternal,
                                base::Unretained(this)));
}

void InProcessUtilityThread::CleanUp() {
  child_process_.reset();

  // See comment in SlavererMainThread.
  SetThreadWasQuitProperly(true);
  g_one_utility_thread_lock.Get().Release();
}

void InProcessUtilityThread::InitInternal() {
  g_one_utility_thread_lock.Get().Acquire();
  child_process_.reset(new ChildProcess());
  child_process_->set_main_thread(new UtilityThreadImpl(params_));
}

base::Thread* CreateInProcessUtilityThread(
    const InProcessChildThreadParams& params) {
  return new InProcessUtilityThread(params);
}

}  // namespace samples
