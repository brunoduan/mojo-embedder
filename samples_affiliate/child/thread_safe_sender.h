// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SAMPLES_CHILD_THREAD_SAFE_SENDER_H_
#define SAMPLES_CHILD_THREAD_SAFE_SENDER_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "samples/common/export.h"
#include "ipc/ipc_sender.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace IPC {
class SyncMessageFilter;
}

namespace samples {
class ChildThreadImpl;

// The class of Sender returned by ChildThreadImpl::thread_safe_sender().
class SAMPLES_EXPORT ThreadSafeSender
    : public IPC::Sender,
      public base::RefCountedThreadSafe<ThreadSafeSender> {
 public:
  bool Send(IPC::Message* msg) override;

 protected:
  ThreadSafeSender(
      const scoped_refptr<base::SingleThreadTaskRunner>& main_task_runner,
      const scoped_refptr<IPC::SyncMessageFilter>& sync_filter);
  ~ThreadSafeSender() override;

 private:
  friend class ChildThreadImpl;  // for construction
  friend class base::RefCountedThreadSafe<ThreadSafeSender>;

  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
  scoped_refptr<IPC::SyncMessageFilter> sync_filter_;

  DISALLOW_COPY_AND_ASSIGN(ThreadSafeSender);
};

}  // namespace samples

#endif  // SAMPLES_CHILD_THREAD_SAFE_SENDER_H_
