// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "samples/child/child_thread_impl.h"

#include <signal.h>
#include <string>
#include <utility>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/debug/alias.h"
#include "base/debug/leak_annotations.h"
#include "base/debug/profiler.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/message_loop/timer_slack.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_macros.h"
#include "base/optional.h"
#include "base/process/process.h"
#include "base/process/process_handle.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_local.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/timer/elapsed_timer.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/tracing/child/child_trace_message_filter.h"
#include "samples/child/child_process.h"
#include "samples/child/thread_safe_sender.h"
#include "samples/common/field_trial_recorder.mojom.h"
#include "samples/common/in_process_child_thread_params.h"
#include "samples/public/common/connection_filter.h"
#include "samples/public/common/samples_client.h"
#include "samples/public/common/samples_features.h"
#include "samples/public/common/samples_switches.h"
#include "samples/public/common/service_manager_connection.h"
#include "samples/public/common/service_names.mojom.h"
#include "samples/public/common/simple_connection_filter.h"
#include "ipc/ipc_channel_mojo.h"
#include "ipc/ipc_logging.h"
#include "ipc/ipc_platform_file.h"
#include "ipc/ipc_sync_channel.h"
#include "ipc/ipc_sync_message_filter.h"
#include "mojo/core/embedder/scoped_ipc_support.h"
#include "mojo/public/cpp/platform/named_platform_channel.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/platform/platform_channel_endpoint.h"
#include "mojo/public/cpp/platform/platform_handle.h"
#include "mojo/public/cpp/system/buffer.h"
#include "mojo/public/cpp/system/invitation.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "services/service_manager/embedder/switches.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "services/service_manager/runner/common/client_util.h"
#include "services/service_manager/sandbox/sandbox_type.h"

#if defined(OS_POSIX)
#include "base/posix/global_descriptors.h"
#include "samples/public/common/samples_descriptors.h"
#endif

namespace samples {
namespace {

// How long to wait for a connection to the master process before giving up.
const int kConnectionTimeoutS = 15;

base::LazyInstance<base::ThreadLocalPointer<ChildThreadImpl>>::DestructorAtExit
    g_lazy_child_thread_impl_tls = LAZY_INSTANCE_INITIALIZER;

// This isn't needed on Windows because there the sandbox's job object
// terminates child processes automatically. For unsandboxed processes (i.e.
// plugins), PluginThread has EnsureTerminateMessageFilter.
#if defined(OS_POSIX)

#if defined(ADDRESS_SANITIZER) || defined(LEAK_SANITIZER) || \
    defined(MEMORY_SANITIZER) || defined(THREAD_SANITIZER) || \
    defined(UNDEFINED_SANITIZER)
// A thread delegate that waits for |duration| and then exits the process
// immediately, without executing finalizers.
class WaitAndExitDelegate : public base::PlatformThread::Delegate {
 public:
  explicit WaitAndExitDelegate(base::TimeDelta duration)
      : duration_(duration) {}

  void ThreadMain() override {
    base::PlatformThread::Sleep(duration_);
    base::Process::TerminateCurrentProcessImmediately(0);
  }

 private:
  const base::TimeDelta duration_;
  DISALLOW_COPY_AND_ASSIGN(WaitAndExitDelegate);
};

bool CreateWaitAndExitThread(base::TimeDelta duration) {
  std::unique_ptr<WaitAndExitDelegate> delegate(
      new WaitAndExitDelegate(duration));

  const bool thread_created =
      base::PlatformThread::CreateNonJoinable(0, delegate.get());
  if (!thread_created)
    return false;

  // A non joinable thread has been created. The thread will either terminate
  // the process or will be terminated by the process. Therefore, keep the
  // delegate object alive for the lifetime of the process.
  WaitAndExitDelegate* leaking_delegate = delegate.release();
  ANNOTATE_LEAKING_OBJECT_PTR(leaking_delegate);
  ignore_result(leaking_delegate);
  return true;
}
#endif

class SuicideOnChannelErrorFilter : public IPC::MessageFilter {
 public:
  // IPC::MessageFilter
  void OnChannelError() override {
    // For slaverer/worker processes:
    // On POSIX, at least, one can install an unload handler which loops
    // forever and leave behind a slaverer process which eats 100% CPU forever.
    //
    // This is because the terminate signals (FrameMsg_BeforeUnload and the
    // error from the IPC sender) are routed to the main message loop but never
    // processed (because that message loop is stuck in V8).
    //
    // One could make the master SIGKILL the slaverers, but that leaves open a
    // large window where a master failure (or a user, manually terminating
    // the master because "it's stuck") will leave behind a process eating all
    // the CPU.
    //
    // So, we install a filter on the sender so that we can process this event
    // here and kill the process.
    base::debug::StopProfiling();
#if defined(ADDRESS_SANITIZER) || defined(LEAK_SANITIZER) || \
    defined(MEMORY_SANITIZER) || defined(THREAD_SANITIZER) || \
    defined(UNDEFINED_SANITIZER)
    // Some sanitizer tools rely on exit handlers (e.g. to run leak detection,
    // or dump code coverage data to disk). Instead of exiting the process
    // immediately, we give it 60 seconds to run exit handlers.
    CHECK(CreateWaitAndExitThread(base::TimeDelta::FromSeconds(60)));
#if defined(LEAK_SANITIZER)
    // Invoke LeakSanitizer early to avoid detecting shutdown-only leaks. If
    // leaks are found, the process will exit here.
    __lsan_do_leak_check();
#endif
#else
    base::Process::TerminateCurrentProcessImmediately(0);
#endif
  }

 protected:
  ~SuicideOnChannelErrorFilter() override {}
};

#endif  // OS(POSIX)

#if defined(OS_ANDROID)
// A class that allows for triggering a clean shutdown from another
// thread through draining the main thread's msg loop.
class QuitClosure {
 public:
  QuitClosure();
  ~QuitClosure() = default;

  void BindToMainThread(base::RepeatingClosure quit_closure);
  void QuitFromNonMainThread();

 private:
  base::Lock lock_;
  base::ConditionVariable cond_var_;
  base::RepeatingClosure closure_;
};

QuitClosure::QuitClosure() : cond_var_(&lock_) {
}

void QuitClosure::BindToMainThread(base::RepeatingClosure quit_closure) {
  base::AutoLock lock(lock_);
  closure_ = std::move(quit_closure);
  cond_var_.Signal();
}

void QuitClosure::QuitFromNonMainThread() {
  base::AutoLock lock(lock_);
  while (closure_.is_null())
    cond_var_.Wait();

  closure_.Run();
}

base::LazyInstance<QuitClosure>::DestructorAtExit g_quit_closure =
    LAZY_INSTANCE_INITIALIZER;
#endif

base::Optional<mojo::IncomingInvitation> InitializeMojoIPCChannel() {
  TRACE_EVENT0("startup", "InitializeMojoIPCChannel");
  mojo::PlatformChannelEndpoint endpoint;
#if defined(OS_POSIX)
  endpoint = mojo::PlatformChannelEndpoint(mojo::PlatformHandle(
      base::ScopedFD(base::GlobalDescriptors::GetInstance()->Get(
          service_manager::kMojoIPCChannel))));
#endif
  // Mojo isn't supported on all child process types.
  // TODO(crbug.com/604282): Support Mojo in the remaining processes.
  if (!endpoint.is_valid())
    return base::nullopt;

  return mojo::IncomingInvitation::Accept(std::move(endpoint));
}

class ChannelBootstrapFilter : public ConnectionFilter {
 public:
  explicit ChannelBootstrapFilter(IPC::mojom::ChannelBootstrapPtrInfo bootstrap)
      : bootstrap_(std::move(bootstrap)) {}

 private:
  // ConnectionFilter:
  void OnBindInterface(const service_manager::BindSourceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle* interface_pipe,
                       service_manager::Connector* connector) override {
    if (source_info.identity.name() != mojom::kMasterServiceName)
      return;

    if (interface_name == IPC::mojom::ChannelBootstrap::Name_) {
      DCHECK(bootstrap_.is_valid());
      mojo::FuseInterface(
          IPC::mojom::ChannelBootstrapRequest(std::move(*interface_pipe)),
          std::move(bootstrap_));
    }
  }

  IPC::mojom::ChannelBootstrapPtrInfo bootstrap_;

  DISALLOW_COPY_AND_ASSIGN(ChannelBootstrapFilter);
};

}  // namespace

ChildThread* ChildThread::Get() {
  return ChildThreadImpl::current();
}

ChildThreadImpl::Options::Options()
    : auto_start_service_manager_connection(true), connect_to_master(false) {}

ChildThreadImpl::Options::Options(const Options& other) = default;

ChildThreadImpl::Options::~Options() {
}

ChildThreadImpl::Options::Builder::Builder() {
}

ChildThreadImpl::Options::Builder&
ChildThreadImpl::Options::Builder::InMasterProcess(
    const InProcessChildThreadParams& params) {
  options_.master_process_io_runner = params.io_runner();
  options_.in_process_service_request_token = params.service_request_token();
  options_.mojo_invitation = params.mojo_invitation();
  return *this;
}

ChildThreadImpl::Options::Builder&
ChildThreadImpl::Options::Builder::AutoStartServiceManagerConnection(
    bool auto_start) {
  options_.auto_start_service_manager_connection = auto_start;
  return *this;
}

ChildThreadImpl::Options::Builder&
ChildThreadImpl::Options::Builder::ConnectToMaster(
    bool connect_to_master_parms) {
  options_.connect_to_master = connect_to_master_parms;
  return *this;
}

ChildThreadImpl::Options::Builder&
ChildThreadImpl::Options::Builder::AddStartupFilter(
    IPC::MessageFilter* filter) {
  options_.startup_filters.push_back(filter);
  return *this;
}

ChildThreadImpl::Options::Builder&
ChildThreadImpl::Options::Builder::IPCTaskRunner(
    scoped_refptr<base::SingleThreadTaskRunner> ipc_task_runner_parms) {
  options_.ipc_task_runner = ipc_task_runner_parms;
  return *this;
}

ChildThreadImpl::Options ChildThreadImpl::Options::Builder::Build() {
  return options_;
}

ChildThreadImpl::ChildThreadMessageRouter::ChildThreadMessageRouter(
    IPC::Sender* sender)
    : sender_(sender) {}

bool ChildThreadImpl::ChildThreadMessageRouter::Send(IPC::Message* msg) {
  return sender_->Send(msg);
}

bool ChildThreadImpl::ChildThreadMessageRouter::RouteMessage(
    const IPC::Message& msg) {
  bool handled = IPC::MessageRouter::RouteMessage(msg);
#if defined(OS_ANDROID)
  if (!handled && msg.is_sync()) {
    IPC::Message* reply = IPC::SyncMessage::GenerateReply(&msg);
    reply->set_reply_error();
    Send(reply);
  }
#endif
  return handled;
}

ChildThreadImpl::ChildThreadImpl(base::RepeatingClosure quit_closure)
    : ChildThreadImpl(std::move(quit_closure), Options::Builder().Build()) {}

ChildThreadImpl::ChildThreadImpl(base::RepeatingClosure quit_closure,
                                 const Options& options)
    : route_provider_binding_(this),
      router_(this),
      quit_closure_(std::move(quit_closure)),
      master_process_io_runner_(options.master_process_io_runner),
      channel_connected_factory_(
          new base::WeakPtrFactory<ChildThreadImpl>(this)),
      ipc_task_runner_(options.ipc_task_runner),
      weak_factory_(this) {
  Init(options);
}

scoped_refptr<base::SingleThreadTaskRunner> ChildThreadImpl::GetIOTaskRunner() {
  if (IsInMasterProcess())
    return master_process_io_runner_;
  return ChildProcess::current()->io_task_runner();
}

void ChildThreadImpl::SetFieldTrialGroup(const std::string& trial_name,
                                         const std::string& group_name) {
  if (field_trial_syncer_)
    field_trial_syncer_->OnSetFieldTrialGroup(trial_name, group_name);
}

void ChildThreadImpl::OnFieldTrialGroupFinalized(
    const std::string& trial_name,
    const std::string& group_name) {
  mojom::FieldTrialRecorderPtr field_trial_recorder;
  GetConnector()->BindInterface(mojom::kMasterServiceName,
                                &field_trial_recorder);
  field_trial_recorder->FieldTrialActivated(trial_name);
}

void ChildThreadImpl::ConnectChannel() {
  DCHECK(GetServiceManagerConnection());
  IPC::mojom::ChannelBootstrapPtr bootstrap;
  mojo::ScopedMessagePipeHandle handle =
      mojo::MakeRequest(&bootstrap).PassMessagePipe();
  GetServiceManagerConnection()->AddConnectionFilter(
      std::make_unique<ChannelBootstrapFilter>(bootstrap.PassInterface()));

  channel_->Init(
      IPC::ChannelMojo::CreateClientFactory(
          std::move(handle), ChildProcess::current()->io_task_runner(),
          ipc_task_runner_ ? ipc_task_runner_
                           : base::ThreadTaskRunnerHandle::Get()),
      true /* create_pipe_now */);
}

void ChildThreadImpl::Init(const Options& options) {
  TRACE_EVENT0("startup", "ChildThreadImpl::Init");
  g_lazy_child_thread_impl_tls.Pointer()->Set(this);
  on_channel_error_called_ = false;
  main_thread_runner_ = base::ThreadTaskRunnerHandle::Get();
#if BUILDFLAG(IPC_MESSAGE_LOG_ENABLED)
  // We must make sure to instantiate the IPC Logger *before* we create the
  // channel, otherwise we can get a callback on the IO thread which creates
  // the logger, and the logger does not like being created on the IO thread.
  IPC::Logging::GetInstance();
#endif

  channel_ = IPC::SyncChannel::Create(
      this, ChildProcess::current()->io_task_runner(),
      ipc_task_runner_ ? ipc_task_runner_ : base::ThreadTaskRunnerHandle::Get(),
      ChildProcess::current()->GetShutDownEvent());
#if BUILDFLAG(IPC_MESSAGE_LOG_ENABLED)
  if (!IsInMasterProcess())
    IPC::Logging::GetInstance()->SetIPCSender(this);
#endif

  mojo::ScopedMessagePipeHandle service_request_pipe;
  if (!IsInMasterProcess()) {
    mojo_ipc_support_.reset(new mojo::core::ScopedIPCSupport(
        GetIOTaskRunner(), mojo::core::ScopedIPCSupport::ShutdownPolicy::FAST));
    base::Optional<mojo::IncomingInvitation> invitation =
        InitializeMojoIPCChannel();

    std::string service_request_token =
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            service_manager::switches::kServiceRequestChannelToken);
    if (!service_request_token.empty() && invitation) {
      service_request_pipe =
          invitation->ExtractMessagePipe(service_request_token);
    }
  } else {
    service_request_pipe = options.mojo_invitation->ExtractMessagePipe(
        options.in_process_service_request_token);
  }

  if (service_request_pipe.is_valid()) {
    service_manager_connection_ = ServiceManagerConnection::Create(
        service_manager::mojom::ServiceRequest(std::move(service_request_pipe)),
        GetIOTaskRunner());
    std::string pkg_name =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kPackageName);
    if (!pkg_name.empty()) {
      ServiceManagerConnection::SetForProcess(std::move(service_manager_connection_));
    }
  }

  sync_message_filter_ = channel_->CreateSyncMessageFilter();
  thread_safe_sender_ =
      new ThreadSafeSender(main_thread_runner_, sync_message_filter_.get());

  auto registry = std::make_unique<service_manager::BinderRegistry>();
  registry->AddInterface(base::Bind(&ChildThreadImpl::OnChildControlRequest,
                                    base::Unretained(this)),
                         base::ThreadTaskRunnerHandle::Get());
  GetServiceManagerConnection()->AddConnectionFilter(
      std::make_unique<SimpleConnectionFilter>(std::move(registry)));

  InitTracing();

#if defined(OS_POSIX)
  // Check that --process-type is specified so we don't do this in unit tests
  // and single-process mode.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kProcessType))
    channel_->AddFilter(new SuicideOnChannelErrorFilter());
#endif

  // Add filters passed here via options.
  for (auto* startup_filter : options.startup_filters) {
    channel_->AddFilter(startup_filter);
  }

  ConnectChannel();

  // This must always be done after ConnectChannel, because ConnectChannel() may
  // add a ConnectionFilter to the connection.
  if (options.auto_start_service_manager_connection &&
      GetServiceManagerConnection() != nullptr) {
    StartServiceManagerConnection();
  }

  int connection_timeout = kConnectionTimeoutS;
  std::string connection_override =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kIPCConnectionTimeout);
  if (!connection_override.empty()) {
    int temp;
    if (base::StringToInt(connection_override, &temp))
      connection_timeout = temp;
  }

  main_thread_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ChildThreadImpl::EnsureConnected,
                     channel_connected_factory_->GetWeakPtr()),
      base::TimeDelta::FromSeconds(connection_timeout));

#if defined(OS_ANDROID)
  g_quit_closure.Get().BindToMainThread(quit_closure_);
#endif

  // In single-process mode, there is no need to synchronize trials to the
  // master process (because it's the same process).
  if (!IsInMasterProcess()) {
    field_trial_syncer_.reset(
        new variations::ChildProcessFieldTrialSyncer(this));
    field_trial_syncer_->InitFieldTrialObserving(
        *base::CommandLine::ForCurrentProcess());
  }
}

void ChildThreadImpl::InitTracing() {
  // In single process mode, master-side tracing and memory will cover the
  // whole process including slaverers.
  if (IsInMasterProcess())
    return;

  // Tracing adds too much overhead to the profiling service. The only
  // way to determine if this is the profiling service is by checking the
  // sandbox type.
  service_manager::SandboxType sandbox_type =
      service_manager::SandboxTypeFromCommandLine(
          *base::CommandLine::ForCurrentProcess());
  if (sandbox_type == service_manager::SANDBOX_TYPE_PROFILING)
    return;

  channel_->AddFilter(new tracing::ChildTraceMessageFilter(
      ChildProcess::current()->io_task_runner()));

  trace_event_agent_ = tracing::TraceEventAgent::Create(
      GetConnector(), false /* request_clock_sync_marker_on_android */);
}

ChildThreadImpl::~ChildThreadImpl() {
#if BUILDFLAG(IPC_MESSAGE_LOG_ENABLED)
  IPC::Logging::GetInstance()->SetIPCSender(NULL);
#endif

  channel_->RemoveFilter(sync_message_filter_.get());

  // The ChannelProxy object caches a pointer to the IPC thread, so need to
  // reset it as it's not guaranteed to outlive this object.
  // NOTE: this also has the side-effect of not closing the main IPC channel to
  // the master process.  This is needed because this is the signal that the
  // master uses to know that this process has died, so we need it to be alive
  // until this process is shut down, and the OS closes the handle
  // automatically.  We used to watch the object handle on Windows to do this,
  // but it wasn't possible to do so on POSIX.
  channel_->ClearIPCTaskRunner();
  g_lazy_child_thread_impl_tls.Pointer()->Set(nullptr);
}

void ChildThreadImpl::Shutdown() {}

bool ChildThreadImpl::ShouldBeDestroyed() {
  return true;
}

void ChildThreadImpl::OnChannelConnected(int32_t peer_pid) {
  channel_connected_factory_.reset();
}

void ChildThreadImpl::OnChannelError() {
  on_channel_error_called_ = true;
  // If this thread runs in the master process, only Thread::Stop should
  // stop its message loop. Otherwise, QuitWhenIdle could race Thread::Stop.
  if (!IsInMasterProcess())
    quit_closure_.Run();
}

bool ChildThreadImpl::Send(IPC::Message* msg) {
  DCHECK(main_thread_runner_->BelongsToCurrentThread());
  if (!channel_) {
    delete msg;
    return false;
  }

  return channel_->Send(msg);
}

ServiceManagerConnection* ChildThreadImpl::GetServiceManagerConnection() {
  if (service_manager_connection_.get() == nullptr) {
    return ServiceManagerConnection::GetForProcess();
  }

  return service_manager_connection_.get();
}

service_manager::Connector* ChildThreadImpl::GetConnector() {
  return GetServiceManagerConnection()->GetConnector();
}

IPC::MessageRouter* ChildThreadImpl::GetRouter() {
  DCHECK(main_thread_runner_->BelongsToCurrentThread());
  return &router_;
}

mojom::RouteProvider* ChildThreadImpl::GetRemoteRouteProvider() {
  if (!remote_route_provider_) {
    DCHECK(channel_);
    channel_->GetRemoteAssociatedInterface(&remote_route_provider_);
  }
  return remote_route_provider_.get();
}

// static
std::unique_ptr<base::SharedMemory> ChildThreadImpl::AllocateSharedMemory(
    size_t buf_size) {
  mojo::ScopedSharedBufferHandle mojo_buf =
      mojo::SharedBufferHandle::Create(buf_size);
  if (!mojo_buf->is_valid()) {
    LOG(WARNING) << "Master failed to allocate shared memory";
    return nullptr;
  }

  base::SharedMemoryHandle shared_buf;
  if (mojo::UnwrapSharedMemoryHandle(std::move(mojo_buf), &shared_buf,
                                     nullptr, nullptr) != MOJO_RESULT_OK) {
    LOG(WARNING) << "Master failed to allocate shared memory";
    return nullptr;
  }

  return std::make_unique<base::SharedMemory>(shared_buf, false);
}

bool ChildThreadImpl::OnMessageReceived(const IPC::Message& msg) {
  if (msg.routing_id() == MSG_ROUTING_CONTROL)
    return OnControlMessageReceived(msg);

  return router_.OnMessageReceived(msg);
}

void ChildThreadImpl::OnAssociatedInterfaceRequest(
    const std::string& interface_name,
    mojo::ScopedInterfaceEndpointHandle handle) {
  if (interface_name == mojom::RouteProvider::Name_) {
    DCHECK(!route_provider_binding_.is_bound());
    route_provider_binding_.Bind(
        mojom::RouteProviderAssociatedRequest(std::move(handle)),
        ipc_task_runner_ ? ipc_task_runner_
                         : base::ThreadTaskRunnerHandle::Get());
  } else {
    LOG(ERROR) << "Request for unknown Channel-associated interface: "
               << interface_name;
  }
}

void ChildThreadImpl::StartServiceManagerConnection() {
  DCHECK(GetServiceManagerConnection());
  GetServiceManagerConnection()->Start();
  GetSamplesClient()->OnServiceManagerConnected(
      GetServiceManagerConnection());
}

bool ChildThreadImpl::OnControlMessageReceived(const IPC::Message& msg) {
  return false;
}

void ChildThreadImpl::ProcessShutdown() {
  quit_closure_.Run();
}

#if BUILDFLAG(IPC_MESSAGE_LOG_ENABLED)
void ChildThreadImpl::SetIPCLoggingEnabled(bool enable) {
  if (enable)
    IPC::Logging::GetInstance()->Enable();
  else
    IPC::Logging::GetInstance()->Disable();
}
#endif  //  IPC_MESSAGE_LOG_ENABLED

void ChildThreadImpl::OnChildControlRequest(
    mojom::ChildControlRequest request) {
  child_control_bindings_.AddBinding(this, std::move(request));
}

ChildThreadImpl* ChildThreadImpl::current() {
  return g_lazy_child_thread_impl_tls.Pointer()->Get();
}

#if defined(OS_ANDROID)
// The method must NOT be called on the child thread itself.
// It may block the child thread if so.
void ChildThreadImpl::ShutdownThread() {
  DCHECK(!ChildThreadImpl::current()) <<
      "this method should NOT be called from child thread itself";
  g_quit_closure.Get().QuitFromNonMainThread();
}
#endif

void ChildThreadImpl::OnProcessFinalRelease() {
  if (on_channel_error_called_)
    return;

  ProcessShutdown();
}

void ChildThreadImpl::EnsureConnected() {
  VLOG(0) << "ChildThreadImpl::EnsureConnected()";
  base::Process::TerminateCurrentProcessImmediately(0);
}

void ChildThreadImpl::GetRoute(
    int32_t routing_id,
    blink::mojom::AssociatedInterfaceProviderAssociatedRequest request) {
  associated_interface_provider_bindings_.AddBinding(
      this, std::move(request), routing_id);
}

void ChildThreadImpl::GetAssociatedInterface(
    const std::string& name,
    blink::mojom::AssociatedInterfaceAssociatedRequest request) {
  int32_t routing_id =
      associated_interface_provider_bindings_.dispatch_context();
  Listener* route = router_.GetRoute(routing_id);
  if (route)
    route->OnAssociatedInterfaceRequest(name, request.PassHandle());
}

bool ChildThreadImpl::IsInMasterProcess() const {
  return static_cast<bool>(master_process_io_runner_);
}

}  // namespace samples
