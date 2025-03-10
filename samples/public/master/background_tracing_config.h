// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SAMPLES_PUBLIC_MASTER_BACKGROUND_TRACING_CONFIG_H_
#define SAMPLES_PUBLIC_MASTER_BACKGROUND_TRACING_CONFIG_H_

#include <memory>

#include "base/trace_event/trace_event_impl.h"
#include "samples/common/export.h"

namespace base {
class DictionaryValue;
}

namespace samples {

// BackgroundTracingConfig is passed to the BackgroundTracingManager to
// setup the trigger rules used to enable/disable background tracing.
class SAMPLES_EXPORT BackgroundTracingConfig {
 public:
  virtual ~BackgroundTracingConfig();

  enum TracingMode {
    PREEMPTIVE,
    REACTIVE,
  };
  TracingMode tracing_mode() const { return tracing_mode_; }

  static std::unique_ptr<BackgroundTracingConfig> FromDict(
      const base::DictionaryValue* dict);

  virtual void IntoDict(base::DictionaryValue* dict) const = 0;

 private:
  friend class BackgroundTracingConfigImpl;
  explicit BackgroundTracingConfig(TracingMode tracing_mode);

  const TracingMode tracing_mode_;
};

}  // namespace samples

#endif  // SAMPLES_PUBLIC_MASTER_BACKGROUND_TRACING_CONFIG_H_
