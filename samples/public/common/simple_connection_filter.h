// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SAMPLES_PUBLIC_COMMON_SIMPLE_CONNECTION_FILTER_H_
#define SAMPLES_PUBLIC_COMMON_SIMPLE_CONNECTION_FILTER_H_

#include <memory>
#include <string>

#include "samples/common/export.h"
#include "samples/public/common/connection_filter.h"
#include "services/service_manager/public/cpp/binder_registry.h"

namespace service_manager {
struct BindSourceInfo;
}

namespace samples {

class SAMPLES_EXPORT SimpleConnectionFilter : public ConnectionFilter {
 public:
  explicit SimpleConnectionFilter(
      std::unique_ptr<service_manager::BinderRegistry> registry);
  ~SimpleConnectionFilter() override;

  // ConnectionFilter:
  void OnBindInterface(const service_manager::BindSourceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle* interface_pipe,
                       service_manager::Connector* connector) override;

 private:
  std::unique_ptr<service_manager::BinderRegistry> registry_;

  DISALLOW_COPY_AND_ASSIGN(SimpleConnectionFilter);
};

class SAMPLES_EXPORT SimpleConnectionFilterWithSourceInfo
    : public ConnectionFilter {
 public:
  explicit SimpleConnectionFilterWithSourceInfo(
      std::unique_ptr<service_manager::BinderRegistryWithArgs<
          const service_manager::BindSourceInfo&>> registry);
  ~SimpleConnectionFilterWithSourceInfo() override;

  // ConnectionFilter:
  void OnBindInterface(const service_manager::BindSourceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle* interface_pipe,
                       service_manager::Connector* connector) override;

 private:
  std::unique_ptr<service_manager::BinderRegistryWithArgs<
      const service_manager::BindSourceInfo&>>
      registry_;

  DISALLOW_COPY_AND_ASSIGN(SimpleConnectionFilterWithSourceInfo);
};

}  // namespace samples

#endif  // SAMPLES_PUBLIC_COMMON_SIMPLE_CONNECTION_FILTER_H_
