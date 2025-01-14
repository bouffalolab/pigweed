// Copyright 2022 The Pigweed Authors
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

// Client binary for the cross-language integration test.
//
// WORK IN PROGRESS, SEE b/228516801
#include "pw_transfer/client.h"

#include <cstddef>

#include "pw_log/log.h"
#include "pw_rpc/integration_testing.h"
#include "pw_status/status.h"
#include "pw_status/try.h"
#include "pw_stream/std_file_stream.h"
#include "pw_sync/binary_semaphore.h"
#include "pw_thread/thread.h"
#include "pw_thread_stl/options.h"
#include "pw_transfer/transfer_thread.h"

namespace pw::transfer {
namespace {

thread::Options& TransferThreadOptions() {
  static thread::stl::Options options;
  return options;
}

// Transfer status, valid only after semaphore is acquired.
//
// We need to bundle the status and semaphore together because a pw_function
// callback can at most capture the reference to one variable (and we need to
// both set the status and release the semaphore).
struct WriteResult {
  Status status = Status::Unknown();
  sync::BinarySemaphore completed;
};

// Create a pw_transfer client, read data from path_to_data, and write it to the
// client using the given transfer_id.
pw::Status SendData(uint32_t transfer_id, const char* path_to_data) {
  std::byte chunk_buffer[512];
  std::byte encode_buffer[512];
  transfer::Thread<2, 2> transfer_thread(chunk_buffer, encode_buffer);
  thread::Thread system_thread(TransferThreadOptions(), transfer_thread);

  pw::transfer::Client client(rpc::integration_test::client(),
                              rpc::integration_test::kChannelId,
                              transfer_thread,
                              /*max_bytes_to_receive=*/256);

  pw::stream::StdFileReader input(path_to_data);

  WriteResult result;

  client.Write(transfer_id, input, [&result](Status status) {
    result.status = status;
    result.completed.release();
  });

  // Waits for the transfer to complete and returns the status.
  // PW_CHECK(completed.try_acquire_for(3s));  How to get this syntax to work?
  result.completed.acquire();

  transfer_thread.Terminate();
  system_thread.join();

  return result.status;
}

}  // namespace
}  // namespace pw::transfer

int main(int argc, char* argv[]) {
  if (!pw::rpc::integration_test::InitializeClient(
           argc, argv, "PORT TRANSFER_ID PATH_TO_DATA")
           .ok()) {
    return 1;
  }

  if (argc != 4) {
    PW_LOG_INFO("Usage: %s PORT TRANSFER_ID PATH_TO_DATA", argv[0]);
    return 1;
  }

  uint32_t transfer_id = static_cast<uint32_t>(std::atoi(argv[2]));
  char* path_to_data = argv[3];

  if (!pw::transfer::SendData(transfer_id, path_to_data).ok()) {
    PW_LOG_INFO("Failed to transfer!");
    return 1;
  }

  return 0;
}
