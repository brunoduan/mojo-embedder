// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ECHO_MASTER_ECHO_MASTER_SERVICE_H_
#define SERVICES_ECHO_MASTER_ECHO_MASTER_SERVICE_H_

#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/test/echo_master/public/mojom/echo_master.mojom.h"

namespace echo_master {

std::unique_ptr<service_manager::Service> CreateEchoMasterService();

class EchoMasterService : public service_manager::Service, public mojom::EchoMaster {
 public:
  EchoMasterService();
  ~EchoMasterService() override;

 private:
  // service_manager::Service:
  void OnStart() override;
  void OnBindInterface(const service_manager::BindSourceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override;

  // mojom::Echo:
  void EchoString(const std::string& input,
                  EchoStringCallback callback) override;
  void Quit() override;

  void BindEchoMasterRequest(mojom::EchoMasterRequest request);

  service_manager::BinderRegistry registry_;
  mojo::BindingSet<mojom::EchoMaster> bindings_;

  DISALLOW_COPY_AND_ASSIGN(EchoMasterService);
};

}  // namespace echo_master

#endif  // SERVICES_ECHO_MASTER_ECHO_MASTER_SERVICE_H_
