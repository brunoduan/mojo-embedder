// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "samples/master/tracing/trace_message_filter.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/task/post_task.h"
#include "components/tracing/common/tracing_messages.h"
#include "samples/master/tracing/background_tracing_manager_impl.h"
#include "samples/common/child_process_host_impl.h"
#include "samples/public/master/master_task_traits.h"
#include "samples/public/master/master_thread.h"

namespace samples {

TraceMessageFilter::TraceMessageFilter(int child_process_id)
    : MasterMessageFilter(TracingMsgStart),
      has_child_(false),
      tracing_process_id_(
          ChildProcessHostImpl::ChildProcessUniqueIdToTracingProcessId(
              child_process_id)) {}

TraceMessageFilter::~TraceMessageFilter() {}

void TraceMessageFilter::OnChannelConnected(int32_t peer_pid) {
  Send(new TracingMsg_SetTracingProcessId(tracing_process_id_));
}

void TraceMessageFilter::OnChannelClosing() {
  if (has_child_) {
    base::PostTaskWithTraits(FROM_HERE, {MasterThread::UI},
                             base::BindOnce(&TraceMessageFilter::Unregister,
                                            base::RetainedRef(this)));
  }
}

bool TraceMessageFilter::OnMessageReceived(const IPC::Message& message) {
  // Always on IO thread (MasterMessageFilter guarantee).
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(TraceMessageFilter, message)
    IPC_MESSAGE_HANDLER(TracingHostMsg_ChildSupportsTracing,
                        OnChildSupportsTracing)
    IPC_MESSAGE_HANDLER(TracingHostMsg_TriggerBackgroundTrace,
                        OnTriggerBackgroundTrace)
    IPC_MESSAGE_HANDLER(TracingHostMsg_AbortBackgroundTrace,
                        OnAbortBackgroundTrace)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void TraceMessageFilter::OnChildSupportsTracing() {
  has_child_ = true;
  base::PostTaskWithTraits(
      FROM_HERE, {MasterThread::UI},
      base::BindOnce(&TraceMessageFilter::Register, base::RetainedRef(this)));
}

void TraceMessageFilter::Register() {
  BackgroundTracingManagerImpl::GetInstance()->AddTraceMessageFilter(this);
}

void TraceMessageFilter::Unregister() {
  BackgroundTracingManagerImpl::GetInstance()->RemoveTraceMessageFilter(this);
}

void TraceMessageFilter::OnTriggerBackgroundTrace(const std::string& name) {
  BackgroundTracingManagerImpl::GetInstance()->OnHistogramTrigger(name);
}

void TraceMessageFilter::OnAbortBackgroundTrace() {
  BackgroundTracingManagerImpl::GetInstance()->AbortScenario();
}

}  // namespace samples
