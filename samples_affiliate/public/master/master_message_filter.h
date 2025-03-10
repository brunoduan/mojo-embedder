// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SAMPLES_PUBLIC_MASTER_MASTER_MESSAGE_FILTER_H_
#define SAMPLES_PUBLIC_MASTER_MASTER_MESSAGE_FILTER_H_

#include <stddef.h>
#include <stdint.h>
#include <utility>

#include "base/memory/ref_counted.h"
#include "base/process/process.h"
#include "build/build_config.h"
#include "samples/common/export.h"
#include "samples/public/master/master_thread.h"
#include "ipc/ipc_channel_proxy.h"

namespace base {
class TaskRunner;
}

namespace IPC {
class MessageFilter;
}

namespace samples {
class MasterAssociatedInterfaceTest;
struct MasterMessageFilterTraits;

// Base class for message filters in the master process.  You can receive and
// send messages on any thread.
class SAMPLES_EXPORT MasterMessageFilter
    : public base::RefCountedThreadSafe<
          MasterMessageFilter, MasterMessageFilterTraits>,
      public IPC::Sender {
 public:
  explicit MasterMessageFilter(uint32_t message_class_to_filter);
  MasterMessageFilter(const uint32_t* message_classes_to_filter,
                       size_t num_message_classes_to_filter);

  // These match the corresponding IPC::MessageFilter methods and are always
  // called on the IO thread.
  virtual void OnFilterAdded(IPC::Channel* channel) {}
  virtual void OnFilterRemoved() {}
  virtual void OnChannelClosing() {}
  virtual void OnChannelError() {}
  virtual void OnChannelConnected(int32_t peer_pid) {}

  // Called when the message filter is about to be deleted.  This gives
  // derived classes the option of controlling which thread they're deleted
  // on etc.
  virtual void OnDestruct() const;

  // IPC::Sender implementation.  Can be called on any thread.  Can't send sync
  // messages (since we don't want to block the master on any other process).
  bool Send(IPC::Message* message) override;

  // If you want the given message to be dispatched to your OnMessageReceived on
  // a different thread, there are two options, either
  // OverrideThreadForMessage or OverrideTaskRunnerForMessage.
  // If neither is overriden, the message will be dispatched on the IO thread.

  // If you want the message to be dispatched on a particular well-known
  // master thread, change |thread| to the id of the target thread
  virtual void OverrideThreadForMessage(
      const IPC::Message& message,
      MasterThread::ID* thread) {}

  // If you want the message to be dispatched via the SequencedWorkerPool,
  // return a non-null task runner which will target tasks accordingly.
  // Note: To target the UI thread, please use OverrideThreadForMessage
  // since that has extra checks to avoid deadlocks.
  virtual base::TaskRunner* OverrideTaskRunnerForMessage(
      const IPC::Message& message);

  // Override this to receive messages.
  // Your function will normally be called on the IO thread.  However, if your
  // OverrideXForMessage modifies the thread used to dispatch the message,
  // your function will be called on the requested thread.
  virtual bool OnMessageReceived(const IPC::Message& message) = 0;

  // Adds an associated interface factory to this filter. Must be called before
  // RegisterAssociatedInterfaces().
  //
  // |filter_removed_callback| is called on the IO thread when this filter is
  // removed.
  void AddAssociatedInterface(
      const std::string& name,
      const IPC::ChannelProxy::GenericAssociatedInterfaceFactory& factory,
      const base::OnceClosure filter_removed_callback);

  // Can be called on any thread, after OnChannelConnected is called.
  base::ProcessHandle PeerHandle();

  // Can be called on any thread, after OnChannelConnected is called.
  base::ProcessId peer_pid() const { return peer_process_.Pid(); }

  void set_peer_process_for_testing(base::Process peer_process) {
    peer_process_ = std::move(peer_process);
  }

  // Called by bad_message.h helpers if a message couldn't be deserialized. This
  // kills the renderer.  Can be called on any thread.  This doesn't log the
  // error details to UMA, so use the bad_message.h for your module instead.
  virtual void ShutdownForBadMessage();

  const std::vector<uint32_t>& message_classes_to_filter() const {
    return message_classes_to_filter_;
  }

 protected:
  ~MasterMessageFilter() override;

 private:
  friend class base::RefCountedThreadSafe<MasterMessageFilter,
                                          MasterMessageFilterTraits>;

  class Internal;
  friend class MasterAssociatedInterfaceTest;
  friend class MasterChildProcessHostImpl;
  friend class SlaverProcessHostImpl;

  // These are private because the only classes that need access to them are
  // made friends above. These are only guaranteed to be valid to call on
  // creation. After that this class could outlive the filter and new interface
  // registrations could race with incoming requests.
  IPC::MessageFilter* GetFilter();
  void RegisterAssociatedInterfaces(IPC::ChannelProxy* proxy);

  // This implements IPC::MessageFilter so that we can hide that from child
  // classes. Internal keeps a reference to this class, which is why there's a
  // weak pointer back. This class could outlive Internal based on what the
  // child class does in its OnDestruct method.
  Internal* internal_;

  IPC::Sender* sender_;
  base::Process peer_process_;

  std::vector<uint32_t> message_classes_to_filter_;

  std::vector<std::pair<std::string,
                        IPC::ChannelProxy::GenericAssociatedInterfaceFactory>>
      associated_interfaces_;

  // Callbacks to be called in OnFilterRemoved().
  std::vector<base::OnceClosure> filter_removed_callbacks_;
};

struct MasterMessageFilterTraits {
  static void Destruct(const MasterMessageFilter* filter) {
    filter->OnDestruct();
  }
};

}  // namespace samples

#endif  // SAMPLES_PUBLIC_MASTER_MASTER_MESSAGE_FILTER_H_
