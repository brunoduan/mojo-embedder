// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "samples/master/child_process_launcher_helper.h"

#include "base/command_line.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/single_thread_task_runner.h"
#include "base/task/lazy_task_runner.h"
#include "base/task/post_task.h"
#include "base/task/single_thread_task_runner_thread_mode.h"
#include "base/task/task_traits.h"
#include "samples/master/child_process_launcher.h"
#include "samples/public/master/master_task_traits.h"
#include "samples/public/master/child_process_launcher_utils.h"
#include "samples/public/common/samples_switches.h"
#include "samples/public/common/sandboxed_process_launcher_delegate.h"
#include "mojo/public/cpp/platform/platform_channel.h"

#if defined(OS_ANDROID)
#include "samples/master/android/launcher_thread.h"
#endif

namespace samples {
namespace internal {

ChildProcessLauncherHelper::Process::Process(Process&& other)
    : process(std::move(other.process))
#if BUILDFLAG(USE_ZYGOTE_HANDLE)
      ,
      zygote(other.zygote)
#endif
{
}

ChildProcessLauncherHelper::Process&
ChildProcessLauncherHelper::Process::Process::operator=(
    ChildProcessLauncherHelper::Process&& other) {
  DCHECK_NE(this, &other);
  process = std::move(other.process);
#if BUILDFLAG(USE_ZYGOTE_HANDLE)
  zygote = other.zygote;
#endif
  return *this;
}

ChildProcessLauncherHelper::ChildProcessLauncherHelper(
    int child_process_id,
    MasterThread::ID client_thread_id,
    std::unique_ptr<base::CommandLine> command_line,
    std::unique_ptr<SandboxedProcessLauncherDelegate> delegate,
    const base::WeakPtr<ChildProcessLauncher>& child_process_launcher,
    bool terminate_on_shutdown,
    mojo::OutgoingInvitation mojo_invitation,
    const mojo::ProcessErrorCallback& process_error_callback)
    : child_process_id_(child_process_id),
      client_thread_id_(client_thread_id),
      command_line_(std::move(command_line)),
      delegate_(std::move(delegate)),
      child_process_launcher_(child_process_launcher),
      terminate_on_shutdown_(terminate_on_shutdown),
      mojo_invitation_(std::move(mojo_invitation)),
      process_error_callback_(process_error_callback) {}

ChildProcessLauncherHelper::~ChildProcessLauncherHelper() = default;

void ChildProcessLauncherHelper::StartLaunchOnClientThread() {
  DCHECK_CURRENTLY_ON(client_thread_id_);

  BeforeLaunchOnClientThread();

  mojo_named_channel_ = CreateNamedPlatformChannelOnClientThread();
  if (!mojo_named_channel_)
    mojo_channel_.emplace();

  GetProcessLauncherTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&ChildProcessLauncherHelper::LaunchOnLauncherThread,
                     this));
}

void ChildProcessLauncherHelper::LaunchOnLauncherThread() {
  DCHECK(CurrentlyOnProcessLauncherTaskRunner());

  begin_launch_time_ = base::TimeTicks::Now();

  std::unique_ptr<FileMappedForLaunch> files_to_register = GetFilesToMap();

  bool is_synchronous_launch = true;
  int launch_result = LAUNCH_RESULT_FAILURE;
  base::LaunchOptions options;

  Process process;
  if (BeforeLaunchOnLauncherThread(*files_to_register, &options)) {
    process =
        LaunchProcessOnLauncherThread(options, std::move(files_to_register),
                                      &is_synchronous_launch, &launch_result);

    AfterLaunchOnLauncherThread(process, options);
  }

  if (is_synchronous_launch) {
    PostLaunchOnLauncherThread(std::move(process), launch_result);
  }
}

void ChildProcessLauncherHelper::PostLaunchOnLauncherThread(
    ChildProcessLauncherHelper::Process process,
    int launch_result) {
  if (mojo_channel_) {
    mojo_channel_->RemoteProcessLaunchAttempted();
  }

  // Take ownership of the broker client invitation here so it's destroyed when
  // we go out of scope regardless of the outcome below.
  mojo::OutgoingInvitation invitation = std::move(mojo_invitation_);
  if (process.process.IsValid()) {
    // Set up Mojo IPC to the new process.
    if (mojo_channel_) {
      DCHECK(mojo_channel_->local_endpoint().is_valid());
      mojo::OutgoingInvitation::Send(
          std::move(invitation), process.process.Handle(),
          mojo_channel_->TakeLocalEndpoint(), process_error_callback_);
    } else {
      DCHECK(mojo_named_channel_);
      mojo::OutgoingInvitation::Send(
          std::move(invitation), process.process.Handle(),
          mojo_named_channel_->TakeServerEndpoint(), process_error_callback_);
    }
  }

  base::PostTaskWithTraits(
      FROM_HERE, {client_thread_id_},
      base::BindOnce(&ChildProcessLauncherHelper::PostLaunchOnClientThread,
                     this, std::move(process), launch_result));
}

void ChildProcessLauncherHelper::PostLaunchOnClientThread(
    ChildProcessLauncherHelper::Process process,
    int error_code) {
  if (child_process_launcher_) {
    child_process_launcher_->Notify(std::move(process), error_code);
  } else if (process.process.IsValid() && terminate_on_shutdown_) {
    // Client is gone, terminate the process.
    ForceNormalProcessTerminationAsync(std::move(process));
  }
}

std::string ChildProcessLauncherHelper::GetProcessType() {
  return command_line()->GetSwitchValueASCII(switches::kProcessType);
}

// static
void ChildProcessLauncherHelper::ForceNormalProcessTerminationAsync(
    ChildProcessLauncherHelper::Process process) {
  if (CurrentlyOnProcessLauncherTaskRunner()) {
    ForceNormalProcessTerminationSync(std::move(process));
    return;
  }
  // On Posix, EnsureProcessTerminated can lead to 2 seconds of sleep!
  // So don't do this on the UI/IO threads.
  GetProcessLauncherTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &ChildProcessLauncherHelper::ForceNormalProcessTerminationSync,
          std::move(process)));
}

}  // namespace internal

// static
base::SingleThreadTaskRunner* GetProcessLauncherTaskRunner() {
#if defined(OS_ANDROID)
  // Android specializes Launcher thread so it is accessible in java.
  // Note Android never does clean shutdown, so shutdown use-after-free
  // concerns are not a problem in practice.
  // This process launcher thread will use the Java-side process-launching
  // thread, instead of creating its own separate thread on C++ side. Note
  // that means this thread will not be joined on shutdown, and may cause
  // use-after-free if anything tries to access objects deleted by
  // AtExitManager, such as non-leaky LazyInstance.
  static base::NoDestructor<scoped_refptr<base::SingleThreadTaskRunner>>
      launcher_task_runner(
          android::LauncherThread::GetMessageLoop()->task_runner());
  return (*launcher_task_runner).get();
#else   // defined(OS_ANDROID)
  // TODO(http://crbug.com/820200): Investigate whether we could use
  // SequencedTaskRunner on platforms other than Windows.
  static base::LazySingleThreadTaskRunner launcher_task_runner =
      LAZY_SINGLE_THREAD_TASK_RUNNER_INITIALIZER(
          base::TaskTraits({base::MayBlock(), base::TaskPriority::USER_BLOCKING,
                            base::TaskShutdownBehavior::BLOCK_SHUTDOWN}),
          base::SingleThreadTaskRunnerThreadMode::DEDICATED);
  return launcher_task_runner.Get().get();
#endif  // defined(OS_ANDROID)
}

// static
bool CurrentlyOnProcessLauncherTaskRunner() {
  return GetProcessLauncherTaskRunner()->RunsTasksInCurrentSequence();
}

}  // namespace samples
