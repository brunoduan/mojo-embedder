// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DATA_DECODER_DATA_DECODER_SERVICE_H_
#define SERVICES_DATA_DECODER_DATA_DECODER_SERVICE_H_

#include <memory>

#include "base/macros.h"
#include "services/data_decoder/public/mojom/json_parser.mojom.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_keepalive.h"

namespace data_decoder {

class DataDecoderService : public service_manager::Service {
 public:
  DataDecoderService();
  ~DataDecoderService() override;

  // Factory function for use as an embedded service.
  static std::unique_ptr<service_manager::Service> Create();

  // service_manager::Service:
  void OnStart() override;
  void OnBindInterface(const service_manager::BindSourceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override;

 private:
  void BindJsonParser(mojom::JsonParserRequest request);

  service_manager::BinderRegistry registry_;
  std::unique_ptr<service_manager::ServiceKeepalive> keepalive_;

  DISALLOW_COPY_AND_ASSIGN(DataDecoderService);
};

}  // namespace data_decoder

#endif  // SERVICES_DATA_DECODER_DATA_DECODER_SERVICE_H_
