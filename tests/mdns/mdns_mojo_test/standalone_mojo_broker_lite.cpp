/*
 *    Copyright (c) 2019, The OpenThread Authors.
 *    All rights reserved.
 *
 *    Redistribution and use in source and binary forms, with or without
 *    modification, are permitted provided that the following conditions are met:
 *    1. Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *    2. Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *    3. Neither the name of the copyright holder nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 *    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *    AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *    ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 *    LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *    CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *    SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *    INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *    CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *    ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *    POSSIBILITY OF SUCH DAMAGE.
 */

#include <string>
#include <vector>

#include <base/command_line.h>
#include "base/at_exit.h"
#include "base/message_loop/message_pump_for_io.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_executor.h"
#include "chromecast/external_mojo/public/cpp/external_mojo_broker.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/core/embedder/scoped_ipc_support.h"

// Standalone Mojo broker process.

int main(int argc, char **argv)
{
    base::AtExitManager exit_manager;
    base::CommandLine::Init(argc, argv);
    base::SingleThreadTaskExecutor io_task_executor(base::MessagePump::Type::IO);
    base::RunLoop                  run_loop;

    mojo::core::Configuration mojo_config;
    mojo_config.is_broker_process = true;
    mojo::core::Init(mojo_config);

    mojo::core::ScopedIPCSupport ipc_support(io_task_executor.task_runner(),
                                             mojo::core::ScopedIPCSupport::ShutdownPolicy::CLEAN);

    chromecast::external_mojo::ExternalMojoBroker broker;

    run_loop.Run();

    return 0;
}
