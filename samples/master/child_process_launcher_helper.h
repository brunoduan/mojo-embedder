// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SAMPLES_MASTER_CHILD_PROCESS_LAUNCHER_HELPER_H_
#define SAMPLES_MASTER_CHILD_PROCESS_LAUNCHER_HELPER_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "base/process/kill.h"
#include "base/process/process.h"
#include "build/build_config.h"
#include "samples/public/master/master_thread.h"
#include "samples/public/common/result_codes.h"
#include "mojo/public/cpp/platform/named_platform_channel.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/system/invitation.h"
#include "services/catalog/public/cpp/manifest_parsing_util.h"
#include "services/service_manager/zygote/common/zygote_buildflags.h"

#if defined(OS_ANDROID)
#include "base/android/scoped_java_ref.h"
#endif

#if defined(OS_WIN)
#include "sandbox/win/src/sandbox_types.h"
#else
#include "samples/public/master/posix_file_descriptor_info.h"
#endif

#if BUILDFLAG(USE_ZYGOTE_HANDLE)
#include "services/service_manager/zygote/common/zygote_handle.h"  // nogncheck
#endif

namespace base {
class CommandLine;
}

namespace samples {

class ChildProcessLauncher;
class SandboxedProcessLauncherDelegate;
struct ChildProcessLauncherPriority;
struct ChildProcessTerminationInfo;

#if defined(OS_POSIX)
class PosixFileDescriptorInfo;
#endif

namespace internal {

#if defined(OS_POSIX)
using FileMappedForLaunch = PosixFileDescriptorInfo;
#else
using FileMappedForLaunch = base::HandlesToInheritVector;
#endif

// ChildProcessLauncherHelper is used by ChildProcessLauncher to start a
// process. Since ChildProcessLauncher can be deleted by its client at any time,
// this class is used to keep state as the process is started asynchronously.
// It also contains the platform specific pieces.
class ChildProcessLauncherHelper :
    public base::RefCountedThreadSafe<ChildProcessLauncherHelper> {
 public:
  // Abstraction around a process required to deal in a platform independent way
  // between Linux (which can use zygotes) and the other platforms.
  struct Process {
    Process() {}
    Process(Process&& other);
    ~Process() {}
    Process& operator=(Process&& other);

    base::Process process;

#if BUILDFLAG(USE_ZYGOTE_HANDLE)
    service_manager::ZygoteHandle zygote = nullptr;
#endif  // BUILDFLAG(USE_ZYGOTE_HANDLE)
  };

  ChildProcessLauncherHelper(
      int child_process_id,
      MasterThread::ID client_thread_id,
      std::unique_ptr<base::CommandLine> command_line,
      std::unique_ptr<SandboxedProcessLauncherDelegate> delegate,
      const base::WeakPtr<ChildProcessLauncher>& child_process_launcher,
      bool terminate_on_shutdown,
      mojo::OutgoingInvitation mojo_invitation,
      const mojo::ProcessErrorCallback& process_error_callback);

  // The methods below are defined in the order they are called.

  // Starts the flow of launching the process.
  void StartLaunchOnClientThread();

  // Platform specific.
  void BeforeLaunchOnClientThread();

  // Called to give implementors a chance at creating a server pipe. Platform-
  // specific. Returns |base::nullopt| if the helper should initialize
  // a regular PlatformChannel for communication instead.
  base::Optional<mojo::NamedPlatformChannel>
  CreateNamedPlatformChannelOnClientThread();

  // Returns the list of files that should be mapped in the child process.
  // Platform specific.
  std::unique_ptr<FileMappedForLaunch> GetFilesToMap();

  // Platform specific, returns success or failure. If failure is returned,
  // LaunchOnLauncherThread will not call LaunchProcessOnLauncherThread and
  // AfterLaunchOnLauncherThread, and the launch_result will be reported as
  // LAUNCH_RESULT_FAILURE.
  bool BeforeLaunchOnLauncherThread(
      const FileMappedForLaunch& files_to_register,
      base::LaunchOptions* options);

  // Does the actual starting of the process.
  // |is_synchronous_launch| is set to false if the starting of the process is
  // asynchonous (this is the case on Android), in which case the returned
  // Process is not valid (and PostLaunchOnLauncherThread() will provide the
  // process once it is available).
  // Platform specific.
  ChildProcessLauncherHelper::Process LaunchProcessOnLauncherThread(
      const base::LaunchOptions& options,
      std::unique_ptr<FileMappedForLaunch> files_to_register,
      bool* is_synchronous_launch,
      int* launch_result);

  // Called right after the process has been launched, whether it was created
  // successfully or not. If the process launch is asynchronous, the process may
  // not yet be created. Platform specific.
  void AfterLaunchOnLauncherThread(
      const ChildProcessLauncherHelper::Process& process,
      const base::LaunchOptions& options);

  // Called once the process has been created, successfully or not.
  void PostLaunchOnLauncherThread(ChildProcessLauncherHelper::Process process,
                                  int launch_result);

  // Posted by PostLaunchOnLauncherThread onto the client thread.
  void PostLaunchOnClientThread(ChildProcessLauncherHelper::Process process,
                                int error_code);

  int client_thread_id() const { return client_thread_id_; }

  // See ChildProcessLauncher::GetChildTerminationInfo for more info.
  ChildProcessTerminationInfo GetTerminationInfo(
      const ChildProcessLauncherHelper::Process& process,
      bool known_dead);

  // Terminates |process|.
  // Returns true if the process was stopped, false if the process had not been
  // started yet or could not be stopped.
  // Note that |exit_code| is not used on Android.
  static bool TerminateProcess(const base::Process& process, int exit_code);

  // Terminates the process with the normal exit code and ensures it has been
  // stopped. By returning a normal exit code this ensures UMA won't treat this
  // as a crash.
  // Returns immediately and perform the work on the launcher thread.
  static void ForceNormalProcessTerminationAsync(
      ChildProcessLauncherHelper::Process process);

  void SetProcessPriorityOnLauncherThread(
      base::Process process,
      const ChildProcessLauncherPriority& priority);

  static void SetRegisteredFilesForService(
      const std::string& service_name,
      catalog::RequiredFileMap required_files);

  static void ResetRegisteredFilesForTesting();

#if defined(OS_ANDROID)
  void OnChildProcessStarted(JNIEnv* env,
                             const base::android::JavaParamRef<jobject>& obj,
                             jint handle);
#endif  // OS_ANDROID

 private:
  friend class base::RefCountedThreadSafe<ChildProcessLauncherHelper>;

  ~ChildProcessLauncherHelper();

  void LaunchOnLauncherThread();

  base::CommandLine* command_line() { return command_line_.get(); }
  int child_process_id() const { return child_process_id_; }

  std::string GetProcessType();

  static void ForceNormalProcessTerminationSync(
      ChildProcessLauncherHelper::Process process);

#if defined(OS_ANDROID)
  void set_java_peer_available_on_client_thread() {
    java_peer_avaiable_on_client_thread_ = true;
  }
#endif

  const int child_process_id_;
  const MasterThread::ID client_thread_id_;
  base::TimeTicks begin_launch_time_;
  std::unique_ptr<base::CommandLine> command_line_;
  std::unique_ptr<SandboxedProcessLauncherDelegate> delegate_;
  base::WeakPtr<ChildProcessLauncher> child_process_launcher_;

  // The PlatformChannel that will be used to transmit an invitation to the
  // child process in most cases. Only used if the platform's helper
  // implementation doesn't return a server endpoint from
  // |CreateNamedPlatformChannelOnClientThread()|.
  base::Optional<mojo::PlatformChannel> mojo_channel_;

  // May be used in exclusion to the above if the platform helper implementation
  // returns a valid server endpoint from
  // |CreateNamedPlatformChannelOnClientThread()|.
  base::Optional<mojo::NamedPlatformChannel> mojo_named_channel_;

  bool terminate_on_shutdown_;
  mojo::OutgoingInvitation mojo_invitation_;
  const mojo::ProcessErrorCallback process_error_callback_;

#if defined(OS_ANDROID)
  base::android::ScopedJavaGlobalRef<jobject> java_peer_;
  bool java_peer_avaiable_on_client_thread_ = false;
#endif

};

}  // namespace internal

}  // namespace samples

#endif  // SAMPLES_MASTER_CHILD_PROCESS_LAUNCHER_HELPER_H_
