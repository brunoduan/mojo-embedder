// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SAMPLES_MASTER_TRACING_BACKGROUND_TRACING_RULE_H_
#define SAMPLES_MASTER_TRACING_BACKGROUND_TRACING_RULE_H_

#include <memory>

#include "base/macros.h"
#include "base/values.h"
#include "samples/master/tracing/background_tracing_config_impl.h"
#include "samples/common/export.h"

namespace base {
class DictionaryValue;
}  // namespace base

namespace samples {

class SAMPLES_EXPORT BackgroundTracingRule {
 public:
  BackgroundTracingRule();
  explicit BackgroundTracingRule(int trigger_delay);

  virtual ~BackgroundTracingRule();

  void Setup(const base::DictionaryValue* dict);
  BackgroundTracingConfigImpl::CategoryPreset category_preset() const {
    return category_preset_;
  }
  void set_category_preset(
      BackgroundTracingConfigImpl::CategoryPreset category_preset) {
    category_preset_ = category_preset;
  }

  virtual void Install() {}
  virtual void IntoDict(base::DictionaryValue* dict) const;
  virtual bool ShouldTriggerNamedEvent(const std::string& named_event) const;
  virtual void OnHistogramTrigger(const std::string& histogram_name) const {}

  // Seconds from the rule is triggered to finalization should start.
  virtual int GetTraceDelay() const;

  // Probability that we should allow a tigger to  happen.
  double trigger_chance() const { return trigger_chance_; }

  bool stop_tracing_on_repeated_reactive() const {
    return stop_tracing_on_repeated_reactive_;
  }

  static std::unique_ptr<BackgroundTracingRule> CreateRuleFromDict(
      const base::DictionaryValue* dict);

  void SetArgs(const base::DictionaryValue& args) {
    args_ = args.CreateDeepCopy();
  }
  const base::DictionaryValue* args() const { return args_.get(); }

 private:
  DISALLOW_COPY_AND_ASSIGN(BackgroundTracingRule);

  double trigger_chance_;
  int trigger_delay_;
  bool stop_tracing_on_repeated_reactive_;
  BackgroundTracingConfigImpl::CategoryPreset category_preset_;
  std::unique_ptr<base::DictionaryValue> args_;
};

}  // namespace samples

#endif  // SAMPLES_MASTER_TRACING_BACKGROUND_TRACING_RULE_H_
