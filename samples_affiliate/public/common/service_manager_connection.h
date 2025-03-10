// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SAMPLES_PUBLIC_COMMON_SERVICE_MANAGER_CONNECTION_H_
#define SAMPLES_PUBLIC_COMMON_SERVICE_MANAGER_CONNECTION_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/sequenced_task_runner.h"
#include "samples/common/export.h"
#include "services/service_manager/embedder/embedded_service_info.h"
#include "services/service_manager/public/cpp/identity.h"
#include "services/service_manager/public/mojom/service.mojom.h"

namespace service_manager {
class Connector;
}

namespace samples {

class ConnectionFilter;

// Encapsulates a connection to a //services/service_manager.
// Access a global instance on the thread the ServiceContext was bound by
// calling Holder::Get().
// Clients can add service_manager::Service implementations whose exposed
// interfaces
// will be exposed to inbound connections to this object's Service.
// Alternatively clients can define named services that will be constructed when
// requests for those service names are received.
// Clients must call any of the registration methods when receiving
// SamplesMasterClient::RegisterInProcessServices().
class SAMPLES_EXPORT ServiceManagerConnection {
 public:
  using ServiceRequestHandler =
      base::RepeatingCallback<void(service_manager::mojom::ServiceRequest)>;
  using ServiceRequestHandlerWithPID =
      base::RepeatingCallback<void(service_manager::mojom::ServiceRequest,
                                   service_manager::mojom::PIDReceiverPtr)>;
  using Factory =
      base::RepeatingCallback<std::unique_ptr<ServiceManagerConnection>(void)>;

  // Stores an instance of |connection| in TLS for the current process. Must be
  // called on the thread the connection was created on.
  static void SetForProcess(
      std::unique_ptr<ServiceManagerConnection> connection);

  // Returns the per-process instance, or nullptr if the Service Manager
  // connection has not yet been bound. Must be called on the thread the
  // connection was created on.
  static ServiceManagerConnection* GetForProcess();

  // Destroys the per-process instance. Must be called on the thread the
  // connection was created on.
  static void DestroyForProcess();

  virtual ~ServiceManagerConnection();

  // Sets the factory used to create the ServiceManagerConnection. This must be
  // called before the ServiceManagerConnection has been created.
  static void SetFactoryForTest(Factory* factory);

  // Creates a ServiceManagerConnection from |request|. The connection binds
  // its interfaces and accept new connections on |io_task_runner| only. Note
  // that no incoming connections are accepted until Start() is called.
  static std::unique_ptr<ServiceManagerConnection> Create(
      service_manager::mojom::ServiceRequest request,
      scoped_refptr<base::SequencedTaskRunner> io_task_runner);

  // Begins accepting incoming connections. Connection filters MUST be added
  // before calling this in order to avoid races. See AddConnectionFilter()
  // below.
  virtual void Start() = 0;

  // Returns the service_manager::Connector received via this connection's
  // Service
  // implementation. Use this to initiate connections as this object's Identity.
  virtual service_manager::Connector* GetConnector() = 0;

  // Sets a closure that is called when the connection is lost. Note that
  // connection may already have been closed, in which case |closure| will be
  // run immediately before returning from this function.
  virtual void SetConnectionLostClosure(const base::Closure& closure) = 0;

  static const int kInvalidConnectionFilterId = 0;

  // Allows the caller to filter inbound connections and/or expose interfaces
  // on them. |filter| may be created on any thread, but will be used and
  // destroyed exclusively on the IO thread (the thread corresponding to
  // |io_task_runner| passed to Create() above.)
  //
  // Connection filters MUST be added before calling Start() in order to avoid
  // races.
  //
  // Returns a unique identifier that can be passed to RemoveConnectionFilter()
  // below.
  virtual int AddConnectionFilter(
      std::unique_ptr<ConnectionFilter> filter) = 0;

  // Removes a filter using the id value returned by AddConnectionFilter().
  // Removal (and destruction) happens asynchronously on the IO thread.
  virtual void RemoveConnectionFilter(int filter_id) = 0;

  // Adds an embedded service to this connection's ServiceFactory.
  // |info| provides details on how to construct new instances of the
  // service when an incoming connection is made to |name|.
  virtual void AddEmbeddedService(
      const std::string& name,
      const service_manager::EmbeddedServiceInfo& info) = 0;

  // Adds a generic ServiceRequestHandler for a given service name. This
  // will be used to satisfy any incoming calls to CreateService() which
  // reference the given name.
  //
  // For in-process services, it is preferable to use |AddEmbeddedService()| as
  // defined above.
  virtual void AddServiceRequestHandler(
      const std::string& name,
      const ServiceRequestHandler& handler) = 0;

  // Similar to above but for registering handlers which want to communicate
  // additional information the process hosting the new service.
  virtual void AddServiceRequestHandlerWithPID(
      const std::string& name,
      const ServiceRequestHandlerWithPID& handler) = 0;
};

}  // namespace samples

#endif  // SAMPLES_PUBLIC_COMMON_SERVICE_MANAGER_CONNECTION_H_
