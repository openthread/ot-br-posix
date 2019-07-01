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

#include <memory>
#include <string>
#include <vector>

#define FROM_HERE 0
#define scoped_refptr std::shared_ptr

namespace chromecast {
namespace mojom {

enum class MdnsResult
{
};

class MdnsResponder
{
public:
    void UnregisterServiceInstance(const std::string &, const std::string &, void *) {}

    void RegisterServiceInstance(const std::string &,
                                 const std::string &,
                                 const std::string &,
                                 int,
                                 const std::vector<std::string> &,
                                 void *)
    {
    }
};

using MdnsResponderPtr = std::unique_ptr<MdnsResponder>;
} // namespace mojom

namespace external_mojo {
class ExternalConnector
{
public:
    static inline void Connect(const char *, void *) {}

    void BindInterface(const char *, chromecast::mojom::MdnsResponderPtr *p) {}

    void set_connection_error_callback(void *) {}
};

inline const char *GetBrokerPath(void)
{
    return "";
}
} // namespace external_mojo

} // namespace chromecast

namespace base {

void *DoNothing()
{
    return nullptr;
}

template <typename T, typename... Args> inline void *BindOnce(T, void *, Args...)
{
    return nullptr;
}

template <typename T> void *BindOnce(T)
{
    return nullptr;
}

inline void *Unretained(void *p)
{
    return p;
}

class SingleThreadTaskRunner
{
public:
    void PostTask(int, void *) {}
};

class CommandLine
{
public:
    static inline void Init(int, char **) {}
};

class AtExitManager
{
};

class MessageLoopForIO
{
public:
    scoped_refptr<SingleThreadTaskRunner> task_runner(void) { return r; }

private:
    scoped_refptr<SingleThreadTaskRunner> r;
};

class Closure
{
public:
    void Run(void) {}
};

class RunLoop
{
public:
    Closure QuitClosure(void) { return c; }

    void Run(void) {}

private:
    Closure c;
};

} // namespace base

namespace mojo {
namespace core {

class ScopedIPCSupport
{
public:
    enum class ShutdownPolicy
    {
        CLEAN
    };

    ScopedIPCSupport(scoped_refptr<base::SingleThreadTaskRunner>, ShutdownPolicy) {}
};

inline void Init(void)
{
}

} // namespace core
} // namespace mojo
