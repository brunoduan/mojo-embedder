// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "samples/master/utility_process_host.h"

#include <utility>

#include "base/base_switches.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/i18n/base_i18n_switches.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/threading/thread.h"
#include "samples/master/master_child_process_host_impl.h"
#include "samples/master/slaverer_host/slaver_process_host_impl.h"
#include "samples/master/service_manager/service_manager_context.h"
#include "samples/master/utility_process_host_client.h"
#include "samples/common/child_process_host_impl.h"
#include "samples/common/in_process_child_thread_params.h"
#include "samples/common/service_manager/child_connection.h"
#include "samples/public/master/master_task_traits.h"
#include "samples/public/master/master_thread.h"
#include "samples/public/master/samples_master_client.h"
#include "samples/public/common/samples_switches.h"
#include "samples/public/common/process_type.h"
#include "samples/public/common/sandboxed_process_launcher_delegate.h"
#include "samples/public/common/service_manager_connection.h"
#include "samples/public/common/service_names.mojom.h"
#include "services/service_manager/embedder/switches.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "services/service_manager/sandbox/features.h"
#include "services/service_manager/sandbox/sandbox_type.h"
#include "services/service_manager/sandbox/switches.h"
#include "services/service_manager/zygote/common/zygote_buildflags.h"


#if BUILDFLAG(USE_ZYGOTE_HANDLE)
#include "services/service_manager/zygote/common/zygote_handle.h"  // nogncheck
#endif

namespace samples {

// NOTE: changes to this class need to be reviewed by the security team.
class UtilitySandboxedProcessLauncherDelegate
    : public SandboxedProcessLauncherDelegate {
 public:
  UtilitySandboxedProcessLauncherDelegate(
      service_manager::SandboxType sandbox_type,
      const base::EnvironmentMap& env,
      const base::CommandLine& cmd_line)
      :
#if defined(OS_POSIX)
        env_(env),
#endif
        sandbox_type_(sandbox_type),
        cmd_line_(cmd_line) {
#if DCHECK_IS_ON()
    bool supported_sandbox_type =
        sandbox_type_ == service_manager::SANDBOX_TYPE_NO_SANDBOX ||
#if defined(OS_WIN)
        sandbox_type_ ==
            service_manager::SANDBOX_TYPE_NO_SANDBOX_AND_ELEVATED_PRIVILEGES ||
        sandbox_type_ == service_manager::SANDBOX_TYPE_XRCOMPOSITING ||
#endif
        sandbox_type_ == service_manager::SANDBOX_TYPE_UTILITY ||
        sandbox_type_ == service_manager::SANDBOX_TYPE_NETWORK ||
        sandbox_type_ == service_manager::SANDBOX_TYPE_CDM ||
        sandbox_type_ == service_manager::SANDBOX_TYPE_PDF_COMPOSITOR ||
        sandbox_type_ == service_manager::SANDBOX_TYPE_PROFILING ||
        sandbox_type_ == service_manager::SANDBOX_TYPE_PPAPI ||
        sandbox_type_ == service_manager::SANDBOX_TYPE_AUDIO;
    DCHECK(supported_sandbox_type);
#endif  // DCHECK_IS_ON()
  }

  ~UtilitySandboxedProcessLauncherDelegate() override {}

#if defined(OS_WIN)
  bool GetAppContainerId(std::string* appcontainer_id) override {
    if (sandbox_type_ == service_manager::SANDBOX_TYPE_XRCOMPOSITING &&
        base::FeatureList::IsEnabled(service_manager::features::kXRSandbox)) {
      *appcontainer_id = base::WideToUTF8(cmd_line_.GetProgram().value());
      return true;
    }
    return false;
  }

  bool DisableDefaultPolicy() override {
    switch (sandbox_type_) {
      case service_manager::SANDBOX_TYPE_AUDIO:
        // Default policy is disabled for audio process to allow audio drivers
        // to read device properties (https://crbug.com/883326).
        return true;
      case service_manager::SANDBOX_TYPE_XRCOMPOSITING:
        return base::FeatureList::IsEnabled(
            service_manager::features::kXRSandbox);
      default:
        return false;
    }
  }

  bool ShouldLaunchElevated() override {
    return sandbox_type_ ==
           service_manager::SANDBOX_TYPE_NO_SANDBOX_AND_ELEVATED_PRIVILEGES;
  }

  bool PreSpawnTarget(sandbox::TargetPolicy* policy) override {
    if (sandbox_type_ == service_manager::SANDBOX_TYPE_XRCOMPOSITING &&
        base::FeatureList::IsEnabled(service_manager::features::kXRSandbox)) {
      // There were issues with some mitigations, causing an inability
      // to load OpenVR and Oculus APIs.
      // TODO(https://crbug.com/881919): Try to harden the XR Compositor sandbox
      // to use mitigations and restrict the token.
      policy->SetProcessMitigations(0);
      policy->SetDelayedProcessMitigations(0);

      std::string appcontainer_id;
      if (!GetAppContainerId(&appcontainer_id)) {
        return false;
      }
      sandbox::ResultCode result =
          service_manager::SandboxWin::AddAppContainerProfileToPolicy(
              cmd_line_, sandbox_type_, appcontainer_id, policy);
      if (result != sandbox::SBOX_ALL_OK) {
        return false;
      }

      // Unprotected token/job.
      policy->SetTokenLevel(sandbox::USER_UNPROTECTED,
                            sandbox::USER_UNPROTECTED);
      service_manager::SandboxWin::SetJobLevel(
          cmd_line_, sandbox::JOB_UNPROTECTED, 0, policy);
    }
    return true;
  }
#endif  // OS_WIN

#if BUILDFLAG(USE_ZYGOTE_HANDLE)
  service_manager::ZygoteHandle GetZygote() override {
    if (service_manager::IsUnsandboxedSandboxType(sandbox_type_) ||
        sandbox_type_ == service_manager::SANDBOX_TYPE_NETWORK ||
        sandbox_type_ == service_manager::SANDBOX_TYPE_AUDIO) {
      return nullptr;
    }
    return service_manager::GetGenericZygote();
  }
#endif  // BUILDFLAG(USE_ZYGOTE_HANDLE)

#if defined(OS_POSIX)
  base::EnvironmentMap GetEnvironment() override { return env_; }
#endif  // OS_POSIX

  service_manager::SandboxType GetSandboxType() override {
    return sandbox_type_;
  }

 private:
#if defined(OS_POSIX)
  base::EnvironmentMap env_;
#endif  // OS_WIN
  service_manager::SandboxType sandbox_type_;
  base::CommandLine cmd_line_;
};

UtilityMainThreadFactoryFunction g_utility_main_thread_factory = nullptr;

void UtilityProcessHost::RegisterUtilityMainThreadFactory(
    UtilityMainThreadFactoryFunction create) {
  g_utility_main_thread_factory = create;
}

UtilityProcessHost::UtilityProcessHost(
    const scoped_refptr<UtilityProcessHostClient>& client,
    const scoped_refptr<base::SequencedTaskRunner>& client_task_runner)
    : client_(client),
      client_task_runner_(client_task_runner),
      sandbox_type_(service_manager::SANDBOX_TYPE_UTILITY),
#if defined(OS_LINUX)
      child_flags_(ChildProcessHost::CHILD_ALLOW_SELF),
#else
      child_flags_(ChildProcessHost::CHILD_NORMAL),
#endif
      started_(false),
      name_(base::ASCIIToUTF16("utility process")),
      weak_ptr_factory_(this) {
  process_.reset(new MasterChildProcessHostImpl(PROCESS_TYPE_UTILITY, this,
                                                 mojom::kUtilityServiceName));
}

UtilityProcessHost::~UtilityProcessHost() {
  DCHECK_CURRENTLY_ON(MasterThread::IO);
}

base::WeakPtr<UtilityProcessHost> UtilityProcessHost::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

bool UtilityProcessHost::Send(IPC::Message* message) {
  if (!StartProcess())
    return false;

  return process_->Send(message);
}

void UtilityProcessHost::SetSandboxType(
    service_manager::SandboxType sandbox_type) {
  DCHECK(sandbox_type != service_manager::SANDBOX_TYPE_INVALID);
  sandbox_type_ = sandbox_type;
}

const ChildProcessData& UtilityProcessHost::GetData() {
  return process_->GetData();
}

#if defined(OS_POSIX)
void UtilityProcessHost::SetEnv(const base::EnvironmentMap& env) {
  env_ = env;
}
#endif

bool UtilityProcessHost::Start() {
  return StartProcess();
}

void UtilityProcessHost::BindInterface(
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  process_->child_connection()->BindInterface(interface_name,
                                              std::move(interface_pipe));
}

void UtilityProcessHost::SetMetricsName(const std::string& metrics_name) {
  metrics_name_ = metrics_name;
}

void UtilityProcessHost::SetName(const base::string16& name) {
  name_ = name;
}

void UtilityProcessHost::SetPackageName(const std::string& name) {
  package_name_ = name;
}

void UtilityProcessHost::SetServiceIdentity(
    const service_manager::Identity& identity) {
  service_identity_ = identity;
}

void UtilityProcessHost::SetLaunchCallback(
    base::OnceCallback<void(base::ProcessId)> callback) {
  DCHECK(!launched_);
  launch_callback_ = std::move(callback);
}

bool UtilityProcessHost::StartProcess() {
  if (started_)
    return true;

  started_ = true;
  process_->SetName(name_);
  process_->SetPackageName(package_name_);
  process_->SetMetricsName(metrics_name_);
  process_->GetHost()->CreateChannelMojo();

  if (SlaverProcessHost::run_slaverer_in_process()) {
    DCHECK(g_utility_main_thread_factory);
    // See comment in SlaverProcessHostImpl::Init() for the background on why we
    // support single process mode this way.
    in_process_thread_.reset(
        g_utility_main_thread_factory(InProcessChildThreadParams(
            base::CreateSingleThreadTaskRunnerWithTraits({MasterThread::IO}),
            process_->GetInProcessMojoInvitation(),
            process_->child_connection()->service_token())));
    in_process_thread_->Start();
  } else {
    const base::CommandLine& master_command_line =
        *base::CommandLine::ForCurrentProcess();

    bool has_cmd_prefix =
        master_command_line.HasSwitch(switches::kUtilityCmdPrefix);

#if defined(OS_ANDROID)
    // readlink("/prof/self/exe") sometimes fails on Android at startup.
    // As a workaround skip calling it here, since the executable name is
    // not needed on Android anyway. See crbug.com/500854.
    std::unique_ptr<base::CommandLine> cmd_line =
        std::make_unique<base::CommandLine>(base::CommandLine::NO_PROGRAM);
#else
    int child_flags = child_flags_;

    // When running under gdb, forking /proc/self/exe ends up forking the gdb
    // executable instead of Chromium. It is almost safe to assume that no
    // updates will happen while a developer is running with
    // |switches::kUtilityCmdPrefix|. See ChildProcessHost::GetChildPath() for
    // a similar case with Valgrind.
    if (has_cmd_prefix)
      child_flags = ChildProcessHost::CHILD_NORMAL;

    base::FilePath exe_path = ChildProcessHost::GetChildPath(child_flags);
    if (exe_path.empty()) {
      NOTREACHED() << "Unable to get utility process binary name.";
      return false;
    }

    std::unique_ptr<base::CommandLine> cmd_line =
        std::make_unique<base::CommandLine>(exe_path);
#endif

    cmd_line->AppendSwitchASCII(switches::kProcessType,
                                switches::kUtilityProcess);
    MasterChildProcessHostImpl::CopyFeatureAndFieldTrialFlags(cmd_line.get());
    MasterChildProcessHostImpl::CopyTraceStartupFlags(cmd_line.get());
    std::string locale = GetSamplesClient()->master()->GetApplicationLocale();
    cmd_line->AppendSwitchASCII(switches::kLang, locale);

#if defined(OS_WIN)
    cmd_line->AppendArg(switches::kPrefetchArgumentOther);
#endif  // defined(OS_WIN)

    service_manager::SetCommandLineFlagsForSandboxType(cmd_line.get(),
                                                       sandbox_type_);

    // Master command-line switches to propagate to the utility process.
    static const char* const kSwitchNames[] = {
      service_manager::switches::kNoSandbox,
      switches::kEnableLogging,
      switches::kForceTextDirection,
      switches::kForceUIDirection,
      switches::kLoggingLevel,
      switches::kProxyServer,
      switches::kUseMockCertVerifierForTesting,
      switches::kUtilityStartupDialog,
      switches::kV,
      switches::kVModule,
#if defined(OS_ANDROID)
      switches::kOrderfileMemoryOptimization,
#endif
    };
    cmd_line->CopySwitchesFrom(master_command_line, kSwitchNames,
                               arraysize(kSwitchNames));

    if (has_cmd_prefix) {
      // Launch the utility child process with some prefix
      // (usually "xterm -e gdb --args").
      cmd_line->PrependWrapper(master_command_line.GetSwitchValueNative(
          switches::kUtilityCmdPrefix));
    }

    const bool is_service = service_identity_.has_value();
    if (is_service) {
      GetSamplesClient()->master()->AdjustUtilityServiceProcessCommandLine(
          *service_identity_, cmd_line.get());
    }

    std::unique_ptr<UtilitySandboxedProcessLauncherDelegate> delegate =
        std::make_unique<UtilitySandboxedProcessLauncherDelegate>(
            sandbox_type_, env_, *cmd_line);
    process_->Launch(std::move(delegate), std::move(cmd_line), true);
  }

  return true;
}

bool UtilityProcessHost::OnMessageReceived(const IPC::Message& message) {
  if (!client_.get())
    return true;

  client_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          base::IgnoreResult(&UtilityProcessHostClient::OnMessageReceived),
          client_.get(), message));

  return true;
}

void UtilityProcessHost::OnProcessLaunched() {
  launched_ = true;
  if (launch_callback_)
    std::move(launch_callback_).Run(process_->GetProcess().Pid());
}

void UtilityProcessHost::OnProcessLaunchFailed(int error_code) {
  if (!client_.get())
    return;

  client_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&UtilityProcessHostClient::OnProcessLaunchFailed, client_,
                     error_code));
}

void UtilityProcessHost::OnProcessCrashed(int exit_code) {
  if (!client_.get())
    return;

  client_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&UtilityProcessHostClient::OnProcessCrashed,
                                client_, exit_code));
}

}  // namespace samples
