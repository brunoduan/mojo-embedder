// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "samples/common/child_process_host_impl.h"

#include <limits>

#include "base/atomic_sequence_num.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/hash.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_math.h"
#include "base/path_service.h"
#include "base/process/process_metrics.h"
#include "base/rand_util.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "samples/public/common/bind_interface_helpers.h"
#include "samples/public/common/child_process_host_delegate.h"
#include "samples/public/common/samples_paths.h"
#include "samples/public/common/samples_switches.h"
#include "ipc/ipc.mojom.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_channel_mojo.h"
#include "ipc/ipc_logging.h"
#include "ipc/message_filter.h"
#include "services/service_manager/public/cpp/interface_provider.h"

namespace {

// Global atomic to generate child process unique IDs.
base::AtomicSequenceNumber g_unique_id;

}  // namespace

namespace samples {

// static
ChildProcessHost* ChildProcessHost::Create(ChildProcessHostDelegate* delegate) {
  return new ChildProcessHostImpl(delegate);
}

// static
base::FilePath ChildProcessHost::GetChildPath(int flags) {
  base::FilePath child_path;

  child_path = base::CommandLine::ForCurrentProcess()->GetSwitchValuePath(
      switches::kMasterSubprocessPath);

#if defined(OS_LINUX)
  // Use /proc/self/exe rather than our known binary path so updates
  // can't swap out the binary from underneath us.
  if (child_path.empty() && flags & CHILD_ALLOW_SELF)
    child_path = base::FilePath(base::kProcSelfExe);
#endif

  // On most platforms, the child executable is the same as the current
  // executable.
  if (child_path.empty())
    base::PathService::Get(CHILD_PROCESS_EXE, &child_path);
  return child_path;
}

ChildProcessHostImpl::ChildProcessHostImpl(ChildProcessHostDelegate* delegate)
    : delegate_(delegate), opening_channel_(false) {}

ChildProcessHostImpl::~ChildProcessHostImpl() {
  // If a channel was never created than it wasn't registered and the filters
  // weren't notified. For the sake of symmetry don't call the matching teardown
  // functions. This is analogous to how RenderProcessHostImpl handles things.
  if (!channel_)
    return;

  for (size_t i = 0; i < filters_.size(); ++i) {
    filters_[i]->OnChannelClosing();
    filters_[i]->OnFilterRemoved();
  }
}

void ChildProcessHostImpl::AddFilter(IPC::MessageFilter* filter) {
  filters_.push_back(filter);

  if (channel_)
    filter->OnFilterAdded(channel_.get());
}

void ChildProcessHostImpl::BindInterface(
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  return delegate_->BindInterface(interface_name, std::move(interface_pipe));
}

void ChildProcessHostImpl::ForceShutdown() {
  child_control_->ProcessShutdown();
}

void ChildProcessHostImpl::CreateChannelMojo() {
  mojo::MessagePipe pipe;
  BindInterface(IPC::mojom::ChannelBootstrap::Name_, std::move(pipe.handle1));
  channel_ = IPC::ChannelMojo::Create(
      std::move(pipe.handle0), IPC::Channel::MODE_SERVER, this,
      base::ThreadTaskRunnerHandle::Get(), base::ThreadTaskRunnerHandle::Get());
  DCHECK(channel_);

  bool initialized = InitChannel();
  DCHECK(initialized);
}

bool ChildProcessHostImpl::InitChannel() {
  if (!channel_->Connect())
    return false;

  for (size_t i = 0; i < filters_.size(); ++i)
    filters_[i]->OnFilterAdded(channel_.get());

  delegate_->OnChannelInitialized(channel_.get());

  // We want to bind this interface as early as possible, but the constructor is
  // too early. |delegate_| may not be fully initialized at that point and thus
  // may be unable to properly fulfill the BindInterface() call. Instead we bind
  // here since the |delegate_| has already been initialized and this is the
  // first potential use of the interface.
  samples::BindInterface(this, &child_control_);

  // Make sure these messages get sent first.
#if BUILDFLAG(IPC_MESSAGE_LOG_ENABLED)
  bool enabled = IPC::Logging::GetInstance()->Enabled();
  child_control_->SetIPCLoggingEnabled(enabled);
#endif

  opening_channel_ = true;

  return true;
}

bool ChildProcessHostImpl::IsChannelOpening() {
  return opening_channel_;
}

bool ChildProcessHostImpl::Send(IPC::Message* message) {
  if (!channel_) {
    delete message;
    return false;
  }
  return channel_->Send(message);
}

int ChildProcessHostImpl::GenerateChildProcessUniqueId() {
  // This function must be threadsafe.
  //
  // Historically, this function returned ids started with 1, so in several
  // places in the code a value of 0 (rather than kInvalidUniqueID) was used as
  // an invalid value. So we retain those semantics.
  int id = g_unique_id.GetNext() + 1;

  CHECK_NE(0, id);
  CHECK_NE(kInvalidUniqueID, id);

  return id;
}

uint64_t ChildProcessHostImpl::ChildProcessUniqueIdToTracingProcessId(
    int child_process_id) {
  // In single process mode, all the children are hosted in the same process,
  // therefore the generated memory dump guids should not be conditioned by the
  // child process id. The clients need not be aware of SPM and the conversion
  // takes care of the SPM special case while translating child process ids to
  // tracing process ids.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kSingleProcess))
    return 0xffffffffffffffffULL;

  // The hash value is incremented so that the tracing id is never equal to
  // MemoryDumpManager::kInvalidTracingProcessId.
  return static_cast<uint64_t>(
             base::Hash(&child_process_id, sizeof(child_process_id))) +
         1;
}

bool ChildProcessHostImpl::OnMessageReceived(const IPC::Message& msg) {
#if BUILDFLAG(IPC_MESSAGE_LOG_ENABLED)
  IPC::Logging* logger = IPC::Logging::GetInstance();
  if (msg.type() == IPC_LOGGING_ID) {
    logger->OnReceivedLoggingMessage(msg);
    return true;
  }

  if (logger->Enabled())
    logger->OnPreDispatchMessage(msg);
#endif

  bool handled = false;
  for (size_t i = 0; i < filters_.size(); ++i) {
    if (filters_[i]->OnMessageReceived(msg)) {
      handled = true;
      break;
    }
  }

  if (!handled) {
      handled = delegate_->OnMessageReceived(msg);
  }

#if BUILDFLAG(IPC_MESSAGE_LOG_ENABLED)
  if (logger->Enabled())
    logger->OnPostDispatchMessage(msg);
#endif
  return handled;
}

void ChildProcessHostImpl::OnChannelConnected(int32_t peer_pid) {
  if (!peer_process_.IsValid()) {
    peer_process_ = base::Process::OpenWithExtraPrivileges(peer_pid);
    if (!peer_process_.IsValid())
       peer_process_ = delegate_->GetProcess().Duplicate();
    DCHECK(peer_process_.IsValid());
  }
  opening_channel_ = false;
  delegate_->OnChannelConnected(peer_pid);
  for (size_t i = 0; i < filters_.size(); ++i)
    filters_[i]->OnChannelConnected(peer_pid);
}

void ChildProcessHostImpl::OnChannelError() {
  opening_channel_ = false;
  delegate_->OnChannelError();

  for (size_t i = 0; i < filters_.size(); ++i)
    filters_[i]->OnChannelError();

  // This will delete host_, which will also destroy this!
  delegate_->OnChildDisconnected();
}

void ChildProcessHostImpl::OnBadMessageReceived(const IPC::Message& message) {
  delegate_->OnBadMessageReceived(message);
}

}  // namespace samples
