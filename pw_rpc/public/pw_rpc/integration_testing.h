// Copyright 2021 The Pigweed Authors
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not
// use this file except in compliance with the License. You may obtain a copy of
// the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
// License for the specific language governing permissions and limitations under
// the License.
#pragma once

#include <cstdint>

#include "pw_function/function.h"
#include "pw_rpc/client.h"
#include "pw_status/status.h"

namespace pw::rpc::integration_test {

// The RPC channel for integration test RPCs.
inline constexpr uint32_t kChannelId = 1;

// An injectable pipe interface that may manipulate packets before they're sent
// to the final destination.
//
// ``ChannelManipulator``s allow application-specific packet handling to be
// injected into the packet processing pipeline for an ingress or egress
// channel-like pathway. This is particularly useful for integration testing
// resilience to things like packet loss on a usually-reliable transport. RPC
// server integrations may provide an opportunity to inject a
// ``ChannelManipulator`` for this use case.
//
// A ``ChannelManipulator`` should not set send_packet_, as the consumer of a
// ``ChannelManipulator`` will use ``send_packet`` to insert the provided
// ``ChannelManipulator`` into a packet processing path.
class ChannelManipulator {
 public:
  // The expected function signature of the send callback used by a
  // ChannelManipulator to forward packets to the final destination.
  //
  // The only argument is a byte span containing the RPC packet that should
  // be sent.
  //
  // Returns:
  //   OK - Packet successfully sent.
  //   Other - Failed to send packet.
  using SendCallback = Function<Status(std::span<const std::byte>)>;

  ChannelManipulator()
      : send_packet_([](std::span<const std::byte>) {
          return Status::Unimplemented();
        }) {}
  virtual ~ChannelManipulator() {}

  // Sets the true send callback that a ChannelManipulator will use to forward
  // packets to the final destination.
  //
  // This should not be used by a ChannelManipulator. The consumer of a
  // ChannelManipulator will set the send callback.
  void set_send_packet(SendCallback&& send) { send_packet_ = std::move(send); }

  // Processes an incoming packet before optionally sending it.
  //
  // Implementations of this method may send the processed packet, multiple
  // packets, or no packets at all via the registered `send_packet()`
  // handler.
  virtual Status ProcessAndSend(std::span<const std::byte> packet) = 0;

 protected:
  Status send_packet(std::span<const std::byte> payload) {
    return send_packet_(payload);
  }

 private:
  Function<Status(std::span<const std::byte>)> send_packet_;
};

void SetEgressChannelManipulator(ChannelManipulator* new_channel_manipulator);

void SetIngressChannelManipulator(ChannelManipulator* new_channel_manipulator);

// Returns the global RPC client for integration test use.
Client& client();

// Initializes logging and the global RPC client for integration testing. Starts
// a background thread that processes incoming.
Status InitializeClient(int argc,
                        char* argv[],
                        const char* usage_args = "PORT");

}  // namespace pw::rpc::integration_test
