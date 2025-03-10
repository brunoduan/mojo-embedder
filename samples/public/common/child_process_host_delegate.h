// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SAMPLES_PUBLIC_COMMON_CHILD_PROCESS_HOST_DELEGATE_H_
#define SAMPLES_PUBLIC_COMMON_CHILD_PROCESS_HOST_DELEGATE_H_

#include <string>

#include "base/process/process.h"
#include "samples/common/export.h"
#include "ipc/ipc_listener.h"
#include "mojo/public/cpp/system/message_pipe.h"

namespace IPC {
class Channel;
}

namespace samples {

// Interface that all users of ChildProcessHost need to provide.
class ChildProcessHostDelegate : public IPC::Listener {
 public:
  ~ChildProcessHostDelegate() override {}

  // Called when the IPC channel for the child process is initialized.
  virtual void OnChannelInitialized(IPC::Channel* channel) {}

  // Called when the child process unexpected closes the IPC channel. Delegates
  // would normally delete the object in this case.
  virtual void OnChildDisconnected() {}

  // Returns a reference to the child process. This can be called only after
  // OnProcessLaunched is called or it will be invalid and may crash.
  virtual const base::Process& GetProcess() const = 0;

  // Binds an interface in the child process.
  virtual void BindInterface(const std::string& interface_name,
                             mojo::ScopedMessagePipeHandle interface_pipe) {}
};

};  // namespace samples

#endif  // SAMPLES_PUBLIC_COMMON_CHILD_PROCESS_HOST_DELEGATE_H_
