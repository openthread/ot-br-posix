#include "assert.hpp"
#include "client_http.hpp"
#include "server_http.hpp"
#include <future>

using namespace std;

using HttpServer = SimpleWeb::Server<SimpleWeb::HTTP>;
using HttpClient = SimpleWeb::Client<SimpleWeb::HTTP>;

int main() {
  // Test ScopeRunner
  {
    SimpleWeb::ScopeRunner scope_runner;
    std::thread cancel_thread;
    {
      ASSERT(scope_runner.count == 0);
      auto lock = scope_runner.continue_lock();
      ASSERT(lock);
      ASSERT(scope_runner.count == 1);
      {
        auto lock = scope_runner.continue_lock();
        ASSERT(lock);
        ASSERT(scope_runner.count == 2);
      }
      ASSERT(scope_runner.count == 1);
      cancel_thread = thread([&scope_runner] {
        scope_runner.stop();
        ASSERT(scope_runner.count == -1);
      });
      this_thread::sleep_for(chrono::milliseconds(500));
      ASSERT(scope_runner.count == 1);
    }
    cancel_thread.join();
    ASSERT(scope_runner.count == -1);
    auto lock = scope_runner.continue_lock();
    ASSERT(!lock);
    scope_runner.stop();
    ASSERT(scope_runner.count == -1);

    scope_runner.count = 0;

    vector<thread> threads;
    for(size_t c = 0; c < 100; ++c) {
      threads.emplace_back([&scope_runner] {
        auto lock = scope_runner.continue_lock();
        ASSERT(scope_runner.count > 0);
      });
    }
    for(auto &thread : threads)
      thread.join();
    ASSERT(scope_runner.count == 0);
  }

  HttpServer server;
  server.config.port = 8080;

  server.resource["^/string$"]["POST"] = [](shared_ptr<HttpServer::Response> response, shared_ptr<HttpServer::Request> request) {
    auto content = request->content.string();
    ASSERT(content == request->content.string());

    *response << "HTTP/1.1 200 OK\r\nContent-Length: " << content.length() << "\r\n\r\n"
              << content;

    ASSERT(!request->remote_endpoint().address().to_string().empty());
    ASSERT(request->remote_endpoint().port() != 0);
  };

  server.resource["^/string/dup$"]["POST"] = [](shared_ptr<HttpServer::Response> response, shared_ptr<HttpServer::Request> request) {
    auto content = request->content.string();

    // Send content twice, before it has a chance to be written to the socket.
    *response << "HTTP/1.1 200 OK\r\nContent-Length: " << (content.length() * 2) << "\r\n\r\n"
              << content;
    response->send();
    *response << content;
    response->send();

    ASSERT(!request->remote_endpoint().address().to_string().empty());
    ASSERT(request->remote_endpoint().port() != 0);
  };

  server.resource["^/string2$"]["POST"] = [](shared_ptr<HttpServer::Response> response, shared_ptr<HttpServer::Request> request) {
    response->write(request->content.string());
  };

  server.resource["^/string3$"]["POST"] = [](shared_ptr<HttpServer::Response> response, shared_ptr<HttpServer::Request> request) {
    stringstream stream;
    stream << request->content.rdbuf();
    response->write(stream);
  };

  server.resource["^/string4$"]["POST"] = [](shared_ptr<HttpServer::Response> response, shared_ptr<HttpServer::Request> /*request*/) {
    response->write(SimpleWeb::StatusCode::client_error_forbidden, {{"Test1", "test2"}, {"tesT3", "test4"}});
  };

  server.resource["^/info$"]["GET"] = [](shared_ptr<HttpServer::Response> response, shared_ptr<HttpServer::Request> request) {
    stringstream content_stream;
    content_stream << request->method << " " << request->path << " " << request->http_version << " ";
    content_stream << request->header.find("test parameter")->second;

    content_stream.seekp(0, ios::end);

    *response << "HTTP/1.1 200 OK\r\nContent-Length: " << content_stream.tellp() << "\r\n\r\n"
              << content_stream.rdbuf();
  };

  server.resource["^/work$"]["GET"] = [](shared_ptr<HttpServer::Response> response, shared_ptr<HttpServer::Request> /*request*/) {
    thread work_thread([response] {
      this_thread::sleep_for(chrono::seconds(5));
      response->write("Work done");
    });
    work_thread.detach();
  };

  server.resource["^/match/([0-9]+)$"]["GET"] = [](shared_ptr<HttpServer::Response> response, shared_ptr<HttpServer::Request> request) {
    string number = request->path_match[1];
    *response << "HTTP/1.1 200 OK\r\nContent-Length: " << number.length() << "\r\n\r\n"
              << number;
  };

  server.resource["^/header$"]["GET"] = [](shared_ptr<HttpServer::Response> response, shared_ptr<HttpServer::Request> request) {
    auto content = request->header.find("test1")->second + request->header.find("test2")->second;

    *response << "HTTP/1.1 200 OK\r\nContent-Length: " << content.length() << "\r\n\r\n"
              << content;
  };

  server.resource["^/query_string$"]["GET"] = [](shared_ptr<HttpServer::Response> response, shared_ptr<HttpServer::Request> request) {
    ASSERT(request->path == "/query_string");
    ASSERT(request->query_string == "testing");
    auto queries = request->parse_query_string();
    auto it = queries.find("Testing");
    ASSERT(it != queries.end() && it->first == "testing" && it->second == "");
    response->write(request->query_string);
  };

  server.resource["^/chunked$"]["POST"] = [](shared_ptr<HttpServer::Response> response, shared_ptr<HttpServer::Request> request) {
    ASSERT(request->path == "/chunked");

    ASSERT(request->content.string() == "SimpleWeb in\r\n\r\nchunks.");

    response->write("6\r\nSimple\r\n3\r\nWeb\r\nE\r\n in\r\n\r\nchunks.\r\n0\r\n\r\n", {{"Transfer-Encoding", "chunked"}});
  };

  server.resource["^/chunked2$"]["POST"] = [](shared_ptr<HttpServer::Response> response, shared_ptr<HttpServer::Request> request) {
    ASSERT(request->path == "/chunked2");

    ASSERT(request->content.string() == "HelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorld");

    response->write("258\r\nHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorld\r\n0\r\n\r\n", {{"Transfer-Encoding", "chunked"}});
  };

  server.resource["^/event-stream1$"]["GET"] = [](shared_ptr<HttpServer::Response> response, shared_ptr<HttpServer::Request> /*request*/) {
    thread work_thread([response] {
      response->close_connection_after_response = true; // Unspecified content length

      // Send header
      promise<bool> header_error;
      response->write({{"Content-Type", "text/event-stream"}});
      response->send([&header_error](const SimpleWeb::error_code &ec) {
        header_error.set_value(static_cast<bool>(ec));
      });
      ASSERT(!header_error.get_future().get());

      *response << "data: 1\n\n";
      promise<bool> error;
      response->send([&error](const SimpleWeb::error_code &ec) {
        error.set_value(static_cast<bool>(ec));
      });
      ASSERT(!error.get_future().get());

      // Write result
      *response << "data: 2\n\n";
    });
    work_thread.detach();
  };

  server.resource["^/event-stream2$"]["GET"] = [](shared_ptr<HttpServer::Response> response, shared_ptr<HttpServer::Request> /*request*/) {
    thread work_thread([response] {
      response->close_connection_after_response = true; // Unspecified content length

      // Send header
      promise<bool> header_error;
      response->write({{"Content-Type", "text/event-stream"}});
      response->send([&header_error](const SimpleWeb::error_code &ec) {
        header_error.set_value(static_cast<bool>(ec));
      });
      ASSERT(!header_error.get_future().get());

      *response << "data: 1\r\n\r\n";
      promise<bool> error;
      response->send([&error](const SimpleWeb::error_code &ec) {
        error.set_value(static_cast<bool>(ec));
      });
      ASSERT(!error.get_future().get());

      // Write result
      *response << "data: 2\r\n\r\n";
    });
    work_thread.detach();
  };

  server.resource["^/session-close$"]["GET"] = [](shared_ptr<HttpServer::Response> response, shared_ptr<HttpServer::Request> /*request*/) {
    response->close_connection_after_response = true; // Unspecified content length
    response->write("test", {{"Session", "close"}});
  };
  server.resource["^/session-close-without-correct-header$"]["GET"] = [](shared_ptr<HttpServer::Response> response, shared_ptr<HttpServer::Request> /*request*/) {
    response->close_connection_after_response = true; // Unspecified content length
    response->write("test");
  };

  server.resource["^/non-standard-line-endings1$"]["GET"] = [](shared_ptr<HttpServer::Response> response, shared_ptr<HttpServer::Request> /*request*/) {
    *response << "HTTP/1.1 200 OK\r\nname: value\n\n";
  };

  server.resource["^/non-standard-line-endings2$"]["GET"] = [](shared_ptr<HttpServer::Response> response, shared_ptr<HttpServer::Request> /*request*/) {
    *response << "HTTP/1.1 200 OK\nname: value\n\n";
  };

  std::string long_response;
  for(int c = 0; c < 1000; ++c)
    long_response += to_string(c);
  server.resource["^/long-response$"]["GET"] = [&long_response](shared_ptr<HttpServer::Response> response, shared_ptr<HttpServer::Request> /*request*/) {
    response->write(long_response, {{"name", "value"}});
  };

  thread server_thread([&server]() {
    // Start server
    server.start();
  });

  this_thread::sleep_for(chrono::seconds(1));

  server.stop();
  server_thread.join();

  server_thread = thread([&server]() {
    // Start server
    server.start();
  });

  this_thread::sleep_for(chrono::seconds(1));

  // Test various request types
  {
    HttpClient client("localhost:8080");
    {
      stringstream output;
      auto r = client.request("POST", "/string", "A string");
      ASSERT(SimpleWeb::status_code(r->status_code) == SimpleWeb::StatusCode::success_ok);
      output << r->content.rdbuf();
      ASSERT(output.str() == "A string");
    }

    {
      auto r = client.request("POST", "/string", "A string");
      ASSERT(SimpleWeb::status_code(r->status_code) == SimpleWeb::StatusCode::success_ok);
      ASSERT(r->content.string() == "A string");
      ASSERT(r->content.string() == "A string");
    }

    {
      stringstream output;
      auto r = client.request("POST", "/string2", "A string");
      ASSERT(SimpleWeb::status_code(r->status_code) == SimpleWeb::StatusCode::success_ok);
      output << r->content.rdbuf();
      ASSERT(output.str() == "A string");
    }

    {
      stringstream output;
      auto r = client.request("POST", "/string3", "A string");
      ASSERT(SimpleWeb::status_code(r->status_code) == SimpleWeb::StatusCode::success_ok);
      output << r->content.rdbuf();
      ASSERT(output.str() == "A string");
    }

    {
      stringstream output;
      auto r = client.request("POST", "/string4", "A string");
      ASSERT(SimpleWeb::status_code(r->status_code) == SimpleWeb::StatusCode::client_error_forbidden);
      ASSERT(r->header.size() == 3);
      ASSERT(r->header.find("test1")->second == "test2");
      ASSERT(r->header.find("tEst3")->second == "test4");
      ASSERT(r->header.find("content-length")->second == "0");
      output << r->content.rdbuf();
      ASSERT(output.str() == "");
    }

    {
      stringstream output;
      stringstream content("A string");
      auto r = client.request("POST", "/string", content);
      output << r->content.rdbuf();
      ASSERT(output.str() == "A string");
    }

    {
      // Test rapid calls to Response::send
      stringstream output;
      stringstream content("A string\n");
      auto r = client.request("POST", "/string/dup", content);
      output << r->content.rdbuf();
      ASSERT(output.str() == "A string\nA string\n");
    }

    {
      stringstream output;
      auto r = client.request("GET", "/info", "", {{"Test Parameter", "test value"}});
      output << r->content.rdbuf();
      ASSERT(output.str() == "GET /info 1.1 test value");
    }

    {
      stringstream output;
      auto r = client.request("GET", "/match/123");
      output << r->content.rdbuf();
      ASSERT(output.str() == "123");
    }
    {
      auto r = client.request("POST", "/chunked", "6\r\nSimple\r\n3\r\nWeb\r\nE\r\n in\r\n\r\nchunks.\r\n0\r\n\r\n", {{"Transfer-Encoding", "chunked"}});
      ASSERT(r->content.string() == "SimpleWeb in\r\n\r\nchunks.");
    }
    {
      auto r = client.request("POST", "/chunked2", "258\r\nHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorld\r\n0\r\n\r\n", {{"Transfer-Encoding", "chunked"}});
      ASSERT(r->content.string() == "HelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorldHelloWorld");
    }

    // Test reconnecting
    for(int c = 0; c < 20; ++c) {
      auto r = client.request("GET", "/session-close");
      ASSERT(r->content.string() == "test");
    }
    for(int c = 0; c < 20; ++c) {
      auto r = client.request("GET", "/session-close-without-correct-header");
      ASSERT(r->content.string() == "test");
    }

    // Test non-standard line endings
    {
      auto r = client.request("GET", "/non-standard-line-endings1");
      ASSERT(r->http_version == "1.1");
      ASSERT(r->status_code == "200 OK");
      ASSERT(r->header.size() == 1);
      ASSERT(r->header.begin()->first == "name");
      ASSERT(r->header.begin()->second == "value");
      ASSERT(r->content.string().empty());
    }
    {
      auto r = client.request("GET", "/non-standard-line-endings2");
      ASSERT(r->http_version == "1.1");
      ASSERT(r->status_code == "200 OK");
      ASSERT(r->header.size() == 1);
      ASSERT(r->header.begin()->first == "name");
      ASSERT(r->header.begin()->second == "value");
      ASSERT(r->content.string().empty());
    }
  }
  {
    HttpClient client("localhost:8080");

    HttpClient::Connection *connection;
    {
      // test performing the stream version of the request methods first
      stringstream output;
      stringstream content("A string");
      auto r = client.request("POST", "/string", content);
      output << r->content.rdbuf();
      ASSERT(output.str() == "A string");
      ASSERT(client.connections.size() == 1);
      connection = client.connections.begin()->get();
    }

    {
      stringstream output;
      auto r = client.request("POST", "/string", "A string");
      output << r->content.rdbuf();
      ASSERT(output.str() == "A string");
      ASSERT(client.connections.size() == 1);
      ASSERT(connection == client.connections.begin()->get());
    }

    {
      stringstream output;
      auto r = client.request("GET", "/header", "", {{"test1", "test"}, {"test2", "ing"}});
      output << r->content.rdbuf();
      ASSERT(output.str() == "testing");
      ASSERT(client.connections.size() == 1);
      ASSERT(connection == client.connections.begin()->get());
    }

    {
      stringstream output;
      auto r = client.request("GET", "/query_string?testing");
      ASSERT(r->content.string() == "testing");
      ASSERT(client.connections.size() == 1);
      ASSERT(connection == client.connections.begin()->get());
    }
  }

  // Test large responses
  {
    {
      HttpClient client("localhost:8080");
      client.config.max_response_streambuf_size = 400;
      bool thrown = false;
      try {
        auto r = client.request("GET", "/long-response");
      }
      catch(...) {
        thrown = true;
      }
      ASSERT(thrown);
    }
    HttpClient client("localhost:8080");
    client.config.max_response_streambuf_size = 400;
    {
      size_t calls = 0;
      bool end = false;
      std::string content;
      client.request("GET", "/long-response", [&calls, &content, &end](shared_ptr<HttpClient::Response> response, const SimpleWeb::error_code &ec) {
        ASSERT(!ec);
        content += response->content.string();
        calls++;
        if(calls == 1)
          ASSERT(response->content.end == false);
        end = response->content.end;
      });
      client.io_service->run();
      ASSERT(content == long_response);
      ASSERT(calls > 2);
      ASSERT(end == true);
    }
    {
      size_t calls = 0;
      std::string content;
      client.request("GET", "/long-response", [&calls, &content](shared_ptr<HttpClient::Response> response, const SimpleWeb::error_code &ec) {
        if(calls == 0)
          ASSERT(!ec);
        content += response->content.string();
        calls++;
        response->close();
      });
      SimpleWeb::restart(*client.io_service);
      client.io_service->run();
      ASSERT(!content.empty());
      ASSERT(calls >= 2);
    }
  }

  // Test client timeout
  {
    HttpClient client("localhost:8080");
    client.config.timeout = 2;
    bool thrown = false;
    try {
      auto r = client.request("GET", "/work");
    }
    catch(...) {
      thrown = true;
    }
    ASSERT(thrown);
  }
  {
    HttpClient client("localhost:8080");
    client.config.timeout = 2;
    bool call = false;
    client.request("GET", "/work", [&call](shared_ptr<HttpClient::Response> /*response*/, const SimpleWeb::error_code &ec) {
      ASSERT(ec);
      call = true;
    });
    SimpleWeb::restart(*client.io_service);
    client.io_service->run();
    ASSERT(call);
  }

  // Test asynchronous requests
  {
    HttpClient client("localhost:8080");
    bool call = false;
    client.request("GET", "/match/123", [&call](shared_ptr<HttpClient::Response> response, const SimpleWeb::error_code &ec) {
      ASSERT(!ec);
      stringstream output;
      output << response->content.rdbuf();
      ASSERT(output.str() == "123");
      call = true;
    });
    client.io_service->run();
    ASSERT(call);

    // Test event-stream
    {
      vector<int> calls(4, 0);
      std::size_t call_num = 0;
      client.request("GET", "/event-stream1", [&calls, &call_num](shared_ptr<HttpClient::Response> response, const SimpleWeb::error_code &ec) {
        calls.at(call_num) = 1;
        if(call_num == 0) {
          ASSERT(response->content.string().empty());
          ASSERT(!ec);
        }
        else if(call_num == 1) {
          ASSERT(response->content.string() == "data: 1\n");
          ASSERT(!ec);
        }
        else if(call_num == 2) {
          ASSERT(response->content.string() == "data: 2\n");
          ASSERT(!ec);
        }
        else if(call_num == 3) {
          ASSERT(response->content.string().empty());
          ASSERT(ec == SimpleWeb::error::eof);
        }
        ++call_num;
      });
      SimpleWeb::restart(*client.io_service);
      client.io_service->run();
      for(auto call : calls)
        ASSERT(call);
    }
    {
      vector<int> calls(4, 0);
      std::size_t call_num = 0;
      client.request("GET", "/event-stream2", [&calls, &call_num](shared_ptr<HttpClient::Response> response, const SimpleWeb::error_code &ec) {
        calls.at(call_num) = 1;
        if(call_num == 0) {
          ASSERT(response->content.string().empty());
          ASSERT(!ec);
        }
        else if(call_num == 1) {
          ASSERT(response->content.string() == "data: 1\n");
          ASSERT(!ec);
        }
        else if(call_num == 2) {
          ASSERT(response->content.string() == "data: 2\n");
          ASSERT(!ec);
        }
        else if(call_num == 3) {
          ASSERT(response->content.string().empty());
          ASSERT(ec == SimpleWeb::error::eof);
        }
        ++call_num;
      });
      SimpleWeb::restart(*client.io_service);
      client.io_service->run();
      for(auto call : calls)
        ASSERT(call);
    }

    // Test concurrent requests from same client
    {
      vector<int> calls(100, 0);
      vector<thread> threads;
      for(size_t c = 0; c < 100; ++c) {
        threads.emplace_back([c, &client, &calls] {
          client.request("GET", "/match/123", [c, &calls](shared_ptr<HttpClient::Response> response, const SimpleWeb::error_code &ec) {
            ASSERT(!ec);
            stringstream output;
            output << response->content.rdbuf();
            ASSERT(output.str() == "123");
            calls[c] = 1;
          });
        });
      }
      for(auto &thread : threads)
        thread.join();
      ASSERT(client.connections.size() == 100);
      SimpleWeb::restart(*client.io_service);
      client.io_service->run();
      ASSERT(client.connections.size() == 1);
      for(auto call : calls)
        ASSERT(call);
    }

    // Test concurrent synchronous request calls from same client
    {
      HttpClient client("localhost:8080");
      {
        vector<int> calls(5, 0);
        vector<thread> threads;
        for(size_t c = 0; c < 5; ++c) {
          threads.emplace_back([c, &client, &calls] {
            try {
              auto r = client.request("GET", "/match/123");
              ASSERT(SimpleWeb::status_code(r->status_code) == SimpleWeb::StatusCode::success_ok);
              ASSERT(r->content.string() == "123");
              calls[c] = 1;
            }
            catch(...) {
              ASSERT(false);
            }
          });
        }
        for(auto &thread : threads)
          thread.join();
        ASSERT(client.connections.size() == 1);
        for(auto call : calls)
          ASSERT(call);
      }
    }

    // Test concurrent requests from different clients
    {
      vector<int> calls(10, 0);
      vector<thread> threads;
      for(size_t c = 0; c < 10; ++c) {
        threads.emplace_back([c, &calls] {
          HttpClient client("localhost:8080");
          client.request("POST", "/string", "A string", [c, &calls](shared_ptr<HttpClient::Response> response, const SimpleWeb::error_code &ec) {
            ASSERT(!ec);
            ASSERT(response->content.string() == "A string");
            calls[c] = 1;
          });
          client.io_service->run();
        });
      }
      for(auto &thread : threads)
        thread.join();
      for(auto call : calls)
        ASSERT(call);
    }
  }

  // Test multiple requests through a persistent connection
  {
    HttpClient client("localhost:8080");
    ASSERT(client.connections.size() == 0);
    for(size_t c = 0; c < 5000; ++c) {
      auto r1 = client.request("POST", "/string", "A string");
      ASSERT(SimpleWeb::status_code(r1->status_code) == SimpleWeb::StatusCode::success_ok);
      ASSERT(r1->content.string() == "A string");
      ASSERT(client.connections.size() == 1);

      stringstream content("A string");
      auto r2 = client.request("POST", "/string", content);
      ASSERT(SimpleWeb::status_code(r2->status_code) == SimpleWeb::StatusCode::success_ok);
      ASSERT(r2->content.string() == "A string");
      ASSERT(client.connections.size() == 1);
    }
  }

  // Test multiple requests through new several client objects
  for(size_t c = 0; c < 100; ++c) {
    {
      HttpClient client("localhost:8080");
      auto r = client.request("POST", "/string", "A string");
      ASSERT(SimpleWeb::status_code(r->status_code) == SimpleWeb::StatusCode::success_ok);
      ASSERT(r->content.string() == "A string");
      ASSERT(client.connections.size() == 1);
    }

    {
      HttpClient client("localhost:8080");
      stringstream content("A string");
      auto r = client.request("POST", "/string", content);
      ASSERT(SimpleWeb::status_code(r->status_code) == SimpleWeb::StatusCode::success_ok);
      ASSERT(r->content.string() == "A string");
      ASSERT(client.connections.size() == 1);
    }
  }

  // Test Client client's stop()
  for(size_t c = 0; c < 40; ++c) {
    auto io_service = make_shared<SimpleWeb::io_context>();
    bool call = false;
    HttpClient client("localhost:8080");
    client.io_service = io_service;
    client.request("GET", "/work", [&call](shared_ptr<HttpClient::Response> /*response*/, const SimpleWeb::error_code &ec) {
      call = true;
      ASSERT(ec);
    });
    thread thread([io_service] {
      io_service->run();
    });
    this_thread::sleep_for(chrono::milliseconds(100));
    client.stop();
    this_thread::sleep_for(chrono::milliseconds(100));
    thread.join();
    ASSERT(call);
  }

  // Test Client destructor that should cancel the client's request
  for(size_t c = 0; c < 40; ++c) {
    auto io_service = make_shared<SimpleWeb::io_context>();
    {
      HttpClient client("localhost:8080");
      client.io_service = io_service;
      client.request("GET", "/work", [](shared_ptr<HttpClient::Response> /*response*/, const SimpleWeb::error_code & /*ec*/) {
        ASSERT(false);
      });
      thread thread([io_service] {
        io_service->run();
      });
      thread.detach();
      this_thread::sleep_for(chrono::milliseconds(100));
    }
    this_thread::sleep_for(chrono::milliseconds(100));
  }

  server.stop();
  server_thread.join();

  // Test server destructor
  {
    auto io_service = make_shared<SimpleWeb::io_context>();
    bool call = false;
    bool client_catch = false;
    {
      HttpServer server;
      server.config.port = 8081;
      server.io_service = io_service;
      server.resource["^/test$"]["GET"] = [&call](shared_ptr<HttpServer::Response> response, shared_ptr<HttpServer::Request> /*request*/) {
        call = true;
        thread sleep_thread([response] {
          this_thread::sleep_for(chrono::seconds(5));
          response->write(SimpleWeb::StatusCode::success_ok, "test");
          response->send([](const SimpleWeb::error_code & /*ec*/) {
            ASSERT(false);
          });
        });
        sleep_thread.detach();
      };
      server.start();
      thread server_thread([io_service] {
        io_service->run();
      });
      server_thread.detach();
      this_thread::sleep_for(chrono::seconds(1));
      thread client_thread([&client_catch] {
        HttpClient client("localhost:8081");
        try {
          auto r = client.request("GET", "/test");
          ASSERT(false);
        }
        catch(...) {
          client_catch = true;
        }
      });
      client_thread.detach();
      this_thread::sleep_for(chrono::seconds(1));
    }
    this_thread::sleep_for(chrono::seconds(5));
    ASSERT(call);
    ASSERT(client_catch);
    io_service->stop();
  }
}
