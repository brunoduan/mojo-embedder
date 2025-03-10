// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "samples/master/bad_message.h"

#include "base/bind.h"
#include "base/debug/dump_without_crashing.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/post_task.h"
#include "samples/public/master/master_message_filter.h"
#include "samples/public/master/master_task_traits.h"
#include "samples/public/master/master_thread.h"
#include "samples/public/master/slaver_process_host.h"

namespace samples {
namespace bad_message {

namespace {

void LogBadMessage(BadMessageReason reason) {
  static auto* bad_message_reason = base::debug::AllocateCrashKeyString(
      "bad_message_reason", base::debug::CrashKeySize::Size32);

  LOG(ERROR) << "Terminating slaverer for bad IPC message, reason " << reason;
  base::debug::SetCrashKeyString(bad_message_reason, base::IntToString(reason));
}

void ReceivedBadMessageOnUIThread(int slaver_process_id,
                                  BadMessageReason reason) {
  DCHECK(MasterThread::CurrentlyOn(MasterThread::UI));
  SlaverProcessHost* host = SlaverProcessHost::FromID(slaver_process_id);
  if (!host)
    return;

  // A dump has already been generated by the caller. Don't generate another.
  host->ShutdownForBadMessage(
      SlaverProcessHost::CrashReportMode::NO_CRASH_DUMP);
}

}  // namespace

void ReceivedBadMessage(SlaverProcessHost* host, BadMessageReason reason) {
  LogBadMessage(reason);
  host->ShutdownForBadMessage(
      SlaverProcessHost::CrashReportMode::GENERATE_CRASH_DUMP);
}

void ReceivedBadMessage(int slaver_process_id, BadMessageReason reason) {
  // We generate a crash dump here since generating one after posting to the UI
  // thread is less useful.
  LogBadMessage(reason);
  base::debug::DumpWithoutCrashing();

  if (!MasterThread::CurrentlyOn(MasterThread::UI)) {
    base::PostTaskWithTraits(FROM_HERE, {MasterThread::UI},
                             base::BindOnce(&ReceivedBadMessageOnUIThread,
                                            slaver_process_id, reason));
    return;
  }
  ReceivedBadMessageOnUIThread(slaver_process_id, reason);
}

void ReceivedBadMessage(MasterMessageFilter* filter, BadMessageReason reason) {
  LogBadMessage(reason);
  filter->ShutdownForBadMessage();
}

base::debug::CrashKeyString* GetMojoErrorCrashKey() {
  static auto* crash_key = base::debug::AllocateCrashKeyString(
      "mojo-message-error", base::debug::CrashKeySize::Size256);
  return crash_key;
}

base::debug::CrashKeyString* GetKilledProcessOriginLockKey() {
  static auto* crash_key = base::debug::AllocateCrashKeyString(
      "killed_process_origin_lock", base::debug::CrashKeySize::Size64);
  return crash_key;
}

base::debug::CrashKeyString* GetRequestedSiteURLKey() {
  static auto* crash_key = base::debug::AllocateCrashKeyString(
      "requested_site_url", base::debug::CrashKeySize::Size64);
  return crash_key;
}

}  // namespace bad_message
}  // namespace samples
