# Benchmarks

A simple benchmark of Simple-Web-Server and a few similar web libraries.

Details:
* Linux distribution: Debian Testing (2019-07-29)
* Linux kernel: 4.19.0-1-amd64
* CPU: Intel(R) Core(TM) i7-2600 CPU @ 3.40GHz
* CPU cores: 4
* The HTTP load generator [httperf](https://github.com/httperf/httperf) is used
to create the benchmark results, with the following arguments:
```sh
httperf --server=localhost --port=3000 --uri=/ --num-conns=20000 --num-calls=200
```

The response messages were made identical.

## Express

[Express](https://expressjs.com/) is a popular Node.js web framework.

Versions:
* Node: v10.15.2
* Express: 4.17.1

Code:
```js
const express = require('express');
const app = express();

app.get('/', (req, res) => {
  res.removeHeader('X-Powered-By');
  res.removeHeader('Connection');
  res.end('Hello World!')
});

const port = 3000;
app.listen(port, () => console.log(`Example app listening on port ${port}!`));
```

Execution:
```sh
NODE_ENV=production node index.js
```

Example results (13659.7 req/s):
```sh
httperf --client=0/1 --server=localhost --port=3000 --uri=/ --send-buffer=4096 --recv-buffer=16384 --num-conns=20000 --num-calls=200
httperf: warning: open file limit > FD_SETSIZE; limiting max. # of open files to FD_SETSIZE
Maximum connect burst length: 1

Total: connections 20000 requests 40000 replies 20000 test-duration 2.928 s

Connection rate: 6829.9 conn/s (0.1 ms/conn, <=1 concurrent connections)
Connection time [ms]: min 0.1 avg 0.1 max 14.8 median 0.5 stddev 0.1
Connection time [ms]: connect 0.0
Connection length [replies/conn]: 1.000

Request rate: 13659.7 req/s (0.1 ms/req)
Request size [B]: 62.0

Reply rate [replies/s]: min 0.0 avg 0.0 max 0.0 stddev 0.0 (0 samples)
Reply time [ms]: response 0.1 transfer 0.0
Reply size [B]: header 76.0 content 12.0 footer 0.0 (total 88.0)
Reply status: 1xx=0 2xx=20000 3xx=0 4xx=0 5xx=0

CPU time [s]: user 0.66 system 2.27 (user 22.4% system 77.5% total 99.9%)
Net I/O: 1414.0 KB/s (11.6*10^6 bps)

Errors: total 20000 client-timo 0 socket-timo 0 connrefused 0 connreset 20000
Errors: fd-unavail 0 addrunavail 0 ftab-full 0 other 0
```

## Hyper

[Hyper](https://hyper.rs/) is a Rust HTTP library that topped the
[TechEmpower Web Framework Benchmarks results](https://www.techempower.com/benchmarks/#section=data-r18&hw=ph&test=plaintext) in 2019-07-09.

Versions:
* rustc: 1.38.0-nightly
* hyper: 0.12

Code (copied from
https://github.com/hyperium/hyper/blob/0.12.x/examples/hello.rs, but removed `pretty_env_logger`
calls due to compilation issues):
```rust
#![deny(warnings)]
extern crate hyper;
// extern crate pretty_env_logger;

use hyper::{Body, Request, Response, Server};
use hyper::service::service_fn_ok;
use hyper::rt::{self, Future};

fn main() {
    // pretty_env_logger::init();
    let addr = ([127, 0, 0, 1], 3000).into();

    let server = Server::bind(&addr)
        .serve(|| {
            // This is the `Service` that will handle the connection.
            // `service_fn_ok` is a helper to convert a function that
            // returns a Response into a `Service`.
            service_fn_ok(move |_: Request<Body>| {
                Response::new(Body::from("Hello World!"))
            })
        })
        .map_err(|e| eprintln!("server error: {}", e));

    println!("Listening on http://{}", addr);

    rt::run(server);
}
```

Compilation and run:
```sh
cargo run --release
```

Example results (60712.3 req/s):
```sh
httperf --client=0/1 --server=localhost --port=3000 --uri=/ --send-buffer=4096 --recv-buffer=16384 --num-conns=20000 --num-calls=200
httperf: warning: open file limit > FD_SETSIZE; limiting max. # of open files to FD_SETSIZE
Maximum connect burst length: 1

Total: connections 20000 requests 4000000 replies 4000000 test-duration 65.884 s

Connection rate: 303.6 conn/s (3.3 ms/conn, <=1 concurrent connections)
Connection time [ms]: min 3.0 avg 3.3 max 11.3 median 3.5 stddev 0.3
Connection time [ms]: connect 0.0
Connection length [replies/conn]: 200.000

Request rate: 60712.3 req/s (0.0 ms/req)
Request size [B]: 62.0

Reply rate [replies/s]: min 58704.0 avg 60732.7 max 62587.7 stddev 1021.7 (13 samples)
Reply time [ms]: response 0.0 transfer 0.0
Reply size [B]: header 76.0 content 12.0 footer 0.0 (total 88.0)
Reply status: 1xx=0 2xx=4000000 3xx=0 4xx=0 5xx=0

CPU time [s]: user 15.91 system 49.97 (user 24.1% system 75.8% total 100.0%)
Net I/O: 8893.4 KB/s (72.9*10^6 bps)

Errors: total 0 client-timo 0 socket-timo 0 connrefused 0 connreset 0
Errors: fd-unavail 0 addrunavail 0 ftab-full 0 other 0
```

## Simple-Web-Server

In these simplistic tests, the performance of Simple-Web-Server is similar to
the Hyper Rust HTTP library, although Hyper seems to be slightly faster more
often than not.

Versions:
* g++: 9.1.0

Code (modified `http_examples.cpp`):
```c++
#include "server_http.hpp"

using HttpServer = SimpleWeb::Server<SimpleWeb::HTTP>;

int main() {
  HttpServer server;
  server.config.port = 3000;

  server.default_resource["GET"] = [](std::shared_ptr<HttpServer::Response> response, std::shared_ptr<HttpServer::Request> /*request*/) {
    response->write("Hello World!", {{"Date", SimpleWeb::Date::to_string(std::chrono::system_clock::now())}});
  };

  server.start();
}
```

Build, compilation and run:
```sh
mkdir build && cd build
CXX=g++-9 CXXFLAGS="-O2 -DNDEBUG -flto" cmake ..
make
./http_examples
```

Example results (60596.3 req/s):
```sh
httperf --client=0/1 --server=localhost --port=3000 --uri=/ --send-buffer=4096 --recv-buffer=16384 --num-conns=20000 --num-calls=200
httperf: warning: open file limit > FD_SETSIZE; limiting max. # of open files to FD_SETSIZE
Maximum connect burst length: 1

Total: connections 20000 requests 4000000 replies 4000000 test-duration 66.011 s

Connection rate: 303.0 conn/s (3.3 ms/conn, <=1 concurrent connections)
Connection time [ms]: min 3.2 avg 3.3 max 8.0 median 3.5 stddev 0.0
Connection time [ms]: connect 0.0
Connection length [replies/conn]: 200.000

Request rate: 60596.3 req/s (0.0 ms/req)
Request size [B]: 62.0

Reply rate [replies/s]: min 60399.6 avg 60596.9 max 60803.8 stddev 130.9 (13 samples)
Reply time [ms]: response 0.0 transfer 0.0
Reply size [B]: header 76.0 content 12.0 footer 0.0 (total 88.0)
Reply status: 1xx=0 2xx=4000000 3xx=0 4xx=0 5xx=0

CPU time [s]: user 16.07 system 49.93 (user 24.3% system 75.6% total 100.0%)
Net I/O: 8876.4 KB/s (72.7*10^6 bps)

Errors: total 0 client-timo 0 socket-timo 0 connrefused 0 connreset 0
Errors: fd-unavail 0 addrunavail 0 ftab-full 0 other 0
```
