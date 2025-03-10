// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "samples/public/master/background_tracing_config.h"

#include "samples/master/tracing/background_tracing_config_impl.h"

namespace samples {

BackgroundTracingConfig::BackgroundTracingConfig(TracingMode tracing_mode)
    : tracing_mode_(tracing_mode) {}

BackgroundTracingConfig::~BackgroundTracingConfig() {}

std::unique_ptr<BackgroundTracingConfig> BackgroundTracingConfig::FromDict(
    const base::DictionaryValue* dict) {
  return BackgroundTracingConfigImpl::FromDict(dict);
}

}  // namespace samples
