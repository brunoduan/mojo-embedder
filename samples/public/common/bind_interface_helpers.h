// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SAMPLES_PUBLIC_COMMON_BIND_INTERFACE_HELPERS_H_
#define SAMPLES_PUBLIC_COMMON_BIND_INTERFACE_HELPERS_H_

#include "mojo/public/cpp/bindings/interface_ptr.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/system/message_pipe.h"

namespace samples {

template <typename Host, typename Interface>
void BindInterface(Host* host, mojo::InterfacePtr<Interface>* ptr) {
  mojo::MessagePipe pipe;
  ptr->Bind(mojo::InterfacePtrInfo<Interface>(std::move(pipe.handle0), 0u));
  host->BindInterface(Interface::Name_, std::move(pipe.handle1));
}
template <typename Host, typename Interface>
void BindInterface(Host* host, mojo::InterfaceRequest<Interface> request) {
  host->BindInterface(Interface::Name_, std::move(request.PassMessagePipe()));
}

}  // namespace

#endif  // SAMPLES_PUBLIC_COMMON_BIND_INTERFACE_HELPERS_H_
