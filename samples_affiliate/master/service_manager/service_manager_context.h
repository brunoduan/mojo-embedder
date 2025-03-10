// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SAMPLES_MASTER_SERVICE_MANAGER_SERVICE_MANAGER_CONTEXT_H_
#define SAMPLES_MASTER_SERVICE_MANAGER_SERVICE_MANAGER_CONTEXT_H_

#include <map>
#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "samples/common/export.h"

namespace base {
class DeferredSequencedTaskRunner;
}

namespace service_manager {
class Connector;
}

namespace samples {

class ServiceManagerConnection;

// ServiceManagerContext manages the master's connection to the ServiceManager,
// hosting a new in-process ServiceManagerContext if the master was not
// launched from an external one.
class SAMPLES_EXPORT ServiceManagerContext {
 public:
  explicit ServiceManagerContext(scoped_refptr<base::SingleThreadTaskRunner>
                                     service_manager_thread_task_runner);

  ~ServiceManagerContext();

  // Returns a service_manager::Connector that can be used on the IO thread.
  static service_manager::Connector* GetConnectorForIOThread();

  // Returns true if there is a valid process for |process_group_name|. Must be
  // called on the IO thread.
  static bool HasValidProcessForProcessGroup(
      const std::string& process_group_name);

  // Starts the master connction to the ServiceManager. It must be called after
  // the MasterMainLoop starts.
  static void StartMasterConnection();

  static base::DeferredSequencedTaskRunner* GetAudioServiceRunner();

 private:
  class InProcessServiceManagerContext;

  scoped_refptr<base::SingleThreadTaskRunner>
      service_manager_thread_task_runner_;
  scoped_refptr<InProcessServiceManagerContext> in_process_context_;
  std::unique_ptr<ServiceManagerConnection> packaged_services_connection_;

  DISALLOW_COPY_AND_ASSIGN(ServiceManagerContext);
};

}  // namespace samples

#endif  // SAMPLES_MASTER_SERVICE_MANAGER_SERVICE_MANAGER_CONTEXT_H_
