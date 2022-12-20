#ifndef SIMPLE_WEB_CLIENT_HTTP_HPP
#define SIMPLE_WEB_CLIENT_HTTP_HPP

#include "asio_compatibility.hpp"
#include "mutex.hpp"
#include "utility.hpp"
#include <future>
#include <limits>
#include <random>
#include <unordered_set>
#include <vector>

namespace SimpleWeb {
  class HeaderEndMatch {
    int crlfcrlf = 0;
    int lflf = 0;

  public:
    /// Match condition for asio::read_until to match both standard and non-standard HTTP header endings.
    std::pair<asio::buffers_iterator<asio::const_buffers_1>, bool> operator()(asio::buffers_iterator<asio::const_buffers_1> begin, asio::buffers_iterator<asio::const_buffers_1> end) {
      auto it = begin;
      for(; it != end; ++it) {
        if(*it == '\n') {
          if(crlfcrlf == 1)
            ++crlfcrlf;
          else if(crlfcrlf == 2)
            crlfcrlf = 0;
          else if(crlfcrlf == 3)
            return {++it, true};
          if(lflf == 0)
            ++lflf;
          else if(lflf == 1)
            return {++it, true};
        }
        else if(*it == '\r') {
          if(crlfcrlf == 0)
            ++crlfcrlf;
          else if(crlfcrlf == 2)
            ++crlfcrlf;
          else
            crlfcrlf = 0;
          lflf = 0;
        }
        else {
          crlfcrlf = 0;
          lflf = 0;
        }
      }
      return {it, false};
    }
  };
} // namespace SimpleWeb
#ifndef ASIO_STANDALONE
namespace boost {
#endif
  namespace asio {
    template <>
    struct is_match_condition<SimpleWeb::HeaderEndMatch> : public std::true_type {};
  } // namespace asio
#ifndef ASIO_STANDALONE
} // namespace boost
#endif

namespace SimpleWeb {
  template <class socket_type>
  class Client;

  template <class socket_type>
  class ClientBase {
  public:
    class Content : public std::istream {
      friend class ClientBase<socket_type>;

    public:
      std::size_t size() noexcept {
        return streambuf.size();
      }
      /// Convenience function to return content as a string.
      std::string string() noexcept {
        return std::string(asio::buffers_begin(streambuf.data()), asio::buffers_end(streambuf.data()));
      }

      /// When true, this is the last response content part from server for the current request.
      bool end = true;

    private:
      asio::streambuf &streambuf;
      Content(asio::streambuf &streambuf) noexcept : std::istream(&streambuf), streambuf(streambuf) {}
    };

  protected:
    class Connection;

  public:
    class Response {
      friend class ClientBase<socket_type>;
      friend class Client<socket_type>;

      class Shared {
      public:
        std::string http_version, status_code;

        CaseInsensitiveMultimap header;
      };

      asio::streambuf streambuf;

      std::shared_ptr<Shared> shared;

      std::weak_ptr<Connection> connection_weak;

      Response(std::size_t max_response_streambuf_size, const std::shared_ptr<Connection> &connection_) noexcept
          : streambuf(max_response_streambuf_size), shared(new Shared()), connection_weak(connection_), http_version(shared->http_version), status_code(shared->status_code), header(shared->header), content(streambuf) {}

      /// Constructs a response object that has empty content, but otherwise is equal to the response parameter
      Response(const Response &response) noexcept
          : streambuf(response.streambuf.max_size()), shared(response.shared), connection_weak(response.connection_weak), http_version(shared->http_version), status_code(shared->status_code), header(shared->header), content(streambuf) {}

    public:
      std::string &http_version, &status_code;

      CaseInsensitiveMultimap &header;

      Content content;

      /// Closes the connection to the server, preventing further response content parts from server.
      void close() noexcept {
        if(auto connection = this->connection_weak.lock())
          connection->close();
      }
    };

    class Config {
      friend class ClientBase<socket_type>;

    private:
      Config() noexcept {}

    public:
      /// Set timeout on requests in seconds. Default value: 0 (no timeout).
      long timeout = 0;
      /// Set connect timeout in seconds. Default value: 0 (Config::timeout is then used instead).
      long timeout_connect = 0;
      /// Maximum size of response stream buffer. Defaults to architecture maximum.
      /// Reaching this limit will result in a message_size error code.
      std::size_t max_response_streambuf_size = (std::numeric_limits<std::size_t>::max)();
      /// Set proxy server (server:port)
      std::string proxy_server;
    };

  protected:
    class Connection : public std::enable_shared_from_this<Connection> {
    public:
      template <typename... Args>
      Connection(std::shared_ptr<ScopeRunner> handler_runner_, Args &&...args) noexcept
          : handler_runner(std::move(handler_runner_)), socket(new socket_type(std::forward<Args>(args)...)) {}

      std::shared_ptr<ScopeRunner> handler_runner;

      std::unique_ptr<socket_type> socket; // Socket must be unique_ptr since asio::ssl::stream<asio::ip::tcp::socket> is not movable
      bool in_use = false;
      bool attempt_reconnect = true;

      std::unique_ptr<asio::steady_timer> timer;

      void close() noexcept {
        error_code ec;
        socket->lowest_layer().shutdown(asio::ip::tcp::socket::shutdown_both, ec);
        socket->lowest_layer().cancel(ec);
      }

      void set_timeout(long seconds) noexcept {
        if(seconds == 0) {
          timer = nullptr;
          return;
        }
        timer = make_steady_timer(*socket, std::chrono::seconds(seconds));
        std::weak_ptr<Connection> self_weak(this->shared_from_this()); // To avoid keeping Connection instance alive longer than needed
        timer->async_wait([self_weak](const error_code &ec) {
          if(!ec) {
            if(auto self = self_weak.lock())
              self->close();
          }
        });
      }

      void cancel_timeout() noexcept {
        if(timer) {
          try {
            timer->cancel();
          }
          catch(...) {
          }
        }
      }
    };

    class Session {
    public:
      Session(std::size_t max_response_streambuf_size, std::shared_ptr<Connection> connection_, std::unique_ptr<asio::streambuf> request_streambuf_) noexcept
          : connection(std::move(connection_)), request_streambuf(std::move(request_streambuf_)), response(new Response(max_response_streambuf_size, connection)) {}

      std::shared_ptr<Connection> connection;
      std::unique_ptr<asio::streambuf> request_streambuf;
      std::shared_ptr<Response> response;
      std::function<void(const error_code &)> callback;
    };

  public:
    /// Set before calling a request function.
    Config config;

    /// If you want to reuse an already created asio::io_service, store its pointer here before calling a request function.
    /// Do not set when using synchronous request functions.
    std::shared_ptr<io_context> io_service;

    /// Convenience function to perform synchronous request. The io_service is started in this function.
    /// Should not be combined with asynchronous request functions.
    /// If you reuse the io_service for other tasks, use the asynchronous request functions instead.
    /// When requesting Server-Sent Events: will throw on error::eof, please use asynchronous request functions instead.
    std::shared_ptr<Response> request(const std::string &method, const std::string &path = {"/"}, string_view content = {}, const CaseInsensitiveMultimap &header = CaseInsensitiveMultimap()) {
      return sync_request(method, path, content, header);
    }

    /// Convenience function to perform synchronous request. The io_service is started in this function.
    /// Should not be combined with asynchronous request functions.
    /// If you reuse the io_service for other tasks, use the asynchronous request functions instead.
    /// When requesting Server-Sent Events: will throw on error::eof, please use asynchronous request functions instead.
    std::shared_ptr<Response> request(const std::string &method, const std::string &path, std::istream &content, const CaseInsensitiveMultimap &header = CaseInsensitiveMultimap()) {
      return sync_request(method, path, content, header);
    }

    /// Asynchronous request where running Client's io_service is required.
    /// Do not use concurrently with the synchronous request functions.
    /// When requesting Server-Sent Events: request_callback might be called more than twice, first call with empty contents on open, and with ec = error::eof on last call
    void request(const std::string &method, const std::string &path, string_view content, const CaseInsensitiveMultimap &header,
                 std::function<void(std::shared_ptr<Response>, const error_code &)> &&request_callback_) {
      auto session = std::make_shared<Session>(config.max_response_streambuf_size, get_connection(), create_request_header(method, path, header));
      std::weak_ptr<Session> session_weak(session); // To avoid keeping session alive longer than needed
      auto request_callback = std::make_shared<std::function<void(std::shared_ptr<Response>, const error_code &)>>(std::move(request_callback_));
      session->callback = [this, session_weak, request_callback](const error_code &ec) {
        if(auto session = session_weak.lock()) {
          if(session->response->content.end) {
            session->connection->cancel_timeout();
            session->connection->in_use = false;
          }
          {
            LockGuard lock(this->connections_mutex);

            // Remove unused connections, but keep one open for HTTP persistent connection:
            std::size_t unused_connections = 0;
            for(auto it = this->connections.begin(); it != this->connections.end();) {
              if(ec && session->connection == *it)
                it = this->connections.erase(it);
              else if((*it)->in_use)
                ++it;
              else {
                ++unused_connections;
                if(unused_connections > 1)
                  it = this->connections.erase(it);
                else
                  ++it;
              }
            }
          }

          if(*request_callback)
            (*request_callback)(session->response, ec);
        }
      };

      std::ostream write_stream(session->request_streambuf.get());
      if(content.size() > 0) {
        auto header_it = header.find("Content-Length");
        if(header_it == header.end()) {
          header_it = header.find("Transfer-Encoding");
          if(header_it == header.end() || header_it->second != "chunked")
            write_stream << "Content-Length: " << content.size() << "\r\n";
        }
      }
      write_stream << "\r\n";
      write_stream.write(content.data(), static_cast<std::streamsize>(content.size()));

      connect(session);
    }

    /// Asynchronous request where running Client's io_service is required.
    /// Do not use concurrently with the synchronous request functions.
    /// When requesting Server-Sent Events: request_callback might be called more than twice, first call with empty contents on open, and with ec = error::eof on last call
    void request(const std::string &method, const std::string &path, string_view content,
                 std::function<void(std::shared_ptr<Response>, const error_code &)> &&request_callback_) {
      request(method, path, content, CaseInsensitiveMultimap(), std::move(request_callback_));
    }

    /// Asynchronous request where running Client's io_service is required.
    /// Do not use concurrently with the synchronous request functions.
    /// When requesting Server-Sent Events: request_callback might be called more than twice, first call with empty contents on open, and with ec = error::eof on last call
    void request(const std::string &method, const std::string &path,
                 std::function<void(std::shared_ptr<Response>, const error_code &)> &&request_callback_) {
      request(method, path, std::string(), CaseInsensitiveMultimap(), std::move(request_callback_));
    }

    /// Asynchronous request where running Client's io_service is required.
    /// Do not use concurrently with the synchronous request functions.
    /// When requesting Server-Sent Events: request_callback might be called more than twice, first call with empty contents on open, and with ec = error::eof on last call
    void request(const std::string &method, std::function<void(std::shared_ptr<Response>, const error_code &)> &&request_callback_) {
      request(method, std::string("/"), std::string(), CaseInsensitiveMultimap(), std::move(request_callback_));
    }

    /// Asynchronous request where running Client's io_service is required.
    /// Do not use concurrently with the synchronous request functions.
    /// When requesting Server-Sent Events: request_callback might be called more than twice, first call with empty contents on open, and with ec = error::eof on last call
    void request(const std::string &method, const std::string &path, std::istream &content, const CaseInsensitiveMultimap &header,
                 std::function<void(std::shared_ptr<Response>, const error_code &)> &&request_callback_) {
      auto session = std::make_shared<Session>(config.max_response_streambuf_size, get_connection(), create_request_header(method, path, header));
      std::weak_ptr<Session> session_weak(session); // To avoid keeping session alive longer than needed
      auto request_callback = std::make_shared<std::function<void(std::shared_ptr<Response>, const error_code &)>>(std::move(request_callback_));
      session->callback = [this, session_weak, request_callback](const error_code &ec) {
        if(auto session = session_weak.lock()) {
          if(session->response->content.end) {
            session->connection->cancel_timeout();
            session->connection->in_use = false;
          }
          {
            LockGuard lock(this->connections_mutex);

            // Remove unused connections, but keep one open for HTTP persistent connection:
            std::size_t unused_connections = 0;
            for(auto it = this->connections.begin(); it != this->connections.end();) {
              if(ec && session->connection == *it)
                it = this->connections.erase(it);
              else if((*it)->in_use)
                ++it;
              else {
                ++unused_connections;
                if(unused_connections > 1)
                  it = this->connections.erase(it);
                else
                  ++it;
              }
            }
          }

          if(*request_callback)
            (*request_callback)(session->response, ec);
        }
      };

      content.seekg(0, std::ios::end);
      auto content_length = content.tellg();
      content.seekg(0, std::ios::beg);
      std::ostream write_stream(session->request_streambuf.get());
      if(content_length > 0) {
        auto header_it = header.find("Content-Length");
        if(header_it == header.end()) {
          header_it = header.find("Transfer-Encoding");
          if(header_it == header.end() || header_it->second != "chunked")
            write_stream << "Content-Length: " << content_length << "\r\n";
        }
      }
      write_stream << "\r\n";
      if(content_length > 0)
        write_stream << content.rdbuf();

      connect(session);
    }

    /// Asynchronous request where running Client's io_service is required.
    /// Do not use concurrently with the synchronous request functions.
    /// When requesting Server-Sent Events: request_callback might be called more than twice, first call with empty contents on open, and with ec = error::eof on last call
    void request(const std::string &method, const std::string &path, std::istream &content,
                 std::function<void(std::shared_ptr<Response>, const error_code &)> &&request_callback_) {
      request(method, path, content, CaseInsensitiveMultimap(), std::move(request_callback_));
    }

    /// Close connections.
    void stop() noexcept {
      LockGuard lock(connections_mutex);
      for(auto it = connections.begin(); it != connections.end();) {
        (*it)->close();
        it = connections.erase(it);
      }
    }

    virtual ~ClientBase() noexcept {
      handler_runner->stop();
      stop();
      if(internal_io_service)
        io_service->stop();
    }

  protected:
    bool internal_io_service = false;

    std::string host;
    unsigned short port;
    unsigned short default_port;

    std::unique_ptr<std::pair<std::string, std::string>> host_port;

    Mutex connections_mutex;
    std::unordered_set<std::shared_ptr<Connection>> connections GUARDED_BY(connections_mutex);

    std::shared_ptr<ScopeRunner> handler_runner;

    Mutex synchronous_request_mutex;
    bool synchronous_request_called GUARDED_BY(synchronous_request_mutex) = false;

    ClientBase(const std::string &host_port, unsigned short default_port) noexcept : default_port(default_port), handler_runner(new ScopeRunner()) {
      auto parsed_host_port = parse_host_port(host_port, default_port);
      host = parsed_host_port.first;
      port = parsed_host_port.second;
    }

    template <typename ContentType>
    std::shared_ptr<Response> sync_request(const std::string &method, const std::string &path, ContentType &content, const CaseInsensitiveMultimap &header) {
      {
        LockGuard lock(synchronous_request_mutex);
        if(!synchronous_request_called) {
          if(io_service) // Throw if io_service already set
            throw make_error_code::make_error_code(errc::operation_not_permitted);
          io_service = std::make_shared<io_context>();
          internal_io_service = true;
          auto io_service_ = io_service;
          std::thread thread([io_service_] {
            auto work = make_work_guard(*io_service_);
            io_service_->run();
          });
          thread.detach();
          synchronous_request_called = true;
        }
      }

      std::shared_ptr<Response> response;
      std::promise<std::shared_ptr<Response>> response_promise;
      auto stop_future_handlers = std::make_shared<bool>(false);
      request(method, path, content, header, [&response, &response_promise, stop_future_handlers](std::shared_ptr<Response> response_, error_code ec) {
        if(*stop_future_handlers)
          return;

        if(!response)
          response = response_;
        else if(!ec) {
          if(response_->streambuf.size() + response->streambuf.size() > response->streambuf.max_size()) {
            ec = make_error_code::make_error_code(errc::message_size);
            response->close();
          }
          else {
            // Move partial response_ content to response:
            auto &source = response_->streambuf;
            auto &target = response->streambuf;
            target.commit(asio::buffer_copy(target.prepare(source.size()), source.data()));
            source.consume(source.size());
          }
        }

        if(ec) {
          response_promise.set_exception(std::make_exception_ptr(system_error(ec)));
          *stop_future_handlers = true;
        }
        else if(response_->content.end)
          response_promise.set_value(response);
      });

      return response_promise.get_future().get();
    }

    std::shared_ptr<Connection> get_connection() noexcept {
      std::shared_ptr<Connection> connection;
      LockGuard lock(connections_mutex);

      if(!io_service) {
        io_service = std::make_shared<io_context>();
        internal_io_service = true;
      }

      for(auto it = connections.begin(); it != connections.end(); ++it) {
        if(!(*it)->in_use) {
          connection = *it;
          break;
        }
      }
      if(!connection) {
        connection = create_connection();
        connections.emplace(connection);
      }
      connection->attempt_reconnect = true;
      connection->in_use = true;

      if(!host_port) {
        if(config.proxy_server.empty())
          host_port = std::unique_ptr<std::pair<std::string, std::string>>(new std::pair<std::string, std::string>(host, std::to_string(port)));
        else {
          auto proxy_host_port = parse_host_port(config.proxy_server, 8080);
          host_port = std::unique_ptr<std::pair<std::string, std::string>>(new std::pair<std::string, std::string>(proxy_host_port.first, std::to_string(proxy_host_port.second)));
        }
      }

      return connection;
    }

    std::pair<std::string, unsigned short> parse_host_port(const std::string &host_port, unsigned short default_port) const noexcept {
      std::string host, port;
      host.reserve(host_port.size());
      bool parse_port = false;
      int square_count = 0; // To parse IPv6 addresses
      for(auto chr : host_port) {
        if(chr == '[')
          ++square_count;
        else if(chr == ']')
          --square_count;
        else if(square_count == 0 && chr == ':')
          parse_port = true;
        else if(!parse_port)
          host += chr;
        else
          port += chr;
      }

      if(port.empty())
        return {std::move(host), default_port};
      else {
        try {
          return {std::move(host), static_cast<unsigned short>(std::stoul(port))};
        }
        catch(...) {
          return {std::move(host), default_port};
        }
      }
    }

    virtual std::shared_ptr<Connection> create_connection() noexcept = 0;
    virtual void connect(const std::shared_ptr<Session> &) = 0;

    std::unique_ptr<asio::streambuf> create_request_header(const std::string &method, const std::string &path, const CaseInsensitiveMultimap &header) const {
      auto corrected_path = path;
      if(corrected_path == "")
        corrected_path = "/";
      if(!config.proxy_server.empty() && std::is_same<socket_type, asio::ip::tcp::socket>::value)
        corrected_path = "http://" + host + ':' + std::to_string(port) + corrected_path;

      std::unique_ptr<asio::streambuf> streambuf(new asio::streambuf());
      std::ostream write_stream(streambuf.get());
      write_stream << method << " " << corrected_path << " HTTP/1.1\r\n";
      write_stream << "Host: " << host;
      if(port != default_port)
        write_stream << ':' << std::to_string(port);
      write_stream << "\r\n";
      for(auto &h : header)
        write_stream << h.first << ": " << h.second << "\r\n";
      return streambuf;
    }

    void write(const std::shared_ptr<Session> &session) {
      session->connection->set_timeout(config.timeout);
      asio::async_write(*session->connection->socket, session->request_streambuf->data(), [this, session](const error_code &ec, std::size_t /*bytes_transferred*/) {
        auto lock = session->connection->handler_runner->continue_lock();
        if(!lock)
          return;
        if(!ec)
          this->read(session);
        else {
          if(session->connection->attempt_reconnect && ec != error::operation_aborted)
            reconnect(session, ec);
          else
            session->callback(ec);
        }
      });
    }

    void read(const std::shared_ptr<Session> &session) {
      asio::async_read_until(*session->connection->socket, session->response->streambuf, HeaderEndMatch(), [this, session](const error_code &ec, std::size_t bytes_transferred) {
        auto lock = session->connection->handler_runner->continue_lock();
        if(!lock)
          return;

        if(!ec) {
          session->connection->attempt_reconnect = true;
          std::size_t num_additional_bytes = session->response->streambuf.size() - bytes_transferred;

          if(!ResponseMessage::parse(session->response->content, session->response->http_version, session->response->status_code, session->response->header)) {
            session->callback(make_error_code::make_error_code(errc::protocol_error));
            return;
          }

          auto header_it = session->response->header.find("Content-Length");
          if(header_it != session->response->header.end()) {
            auto content_length = std::stoull(header_it->second);
            if(content_length > num_additional_bytes)
              this->read_content(session, content_length - num_additional_bytes);
            else
              session->callback(ec);
          }
          else if((header_it = session->response->header.find("Transfer-Encoding")) != session->response->header.end() && header_it->second == "chunked") {
            // Expect hex number to not exceed 16 bytes (64-bit number), but take into account previous additional read bytes
            auto chunk_size_streambuf = std::make_shared<asio::streambuf>(std::max<std::size_t>(16 + 2, session->response->streambuf.size()));

            // Move leftover bytes
            auto &source = session->response->streambuf;
            auto &target = *chunk_size_streambuf;
            target.commit(asio::buffer_copy(target.prepare(source.size()), source.data()));
            source.consume(source.size());

            this->read_chunked_transfer_encoded(session, chunk_size_streambuf);
          }
          else if(session->response->http_version < "1.1" || ((header_it = session->response->header.find("Connection")) != session->response->header.end() && header_it->second == "close"))
            read_content(session);
          else if(((header_it = session->response->header.find("Content-Type")) != session->response->header.end() && header_it->second == "text/event-stream")) {
            auto events_streambuf = std::make_shared<asio::streambuf>(this->config.max_response_streambuf_size);

            // Move leftover bytes
            auto &source = session->response->streambuf;
            auto &target = *events_streambuf;
            target.commit(asio::buffer_copy(target.prepare(source.size()), source.data()));
            source.consume(source.size());

            session->callback(ec); // Connection to a Server-Sent Events resource is opened

            this->read_server_sent_event(session, events_streambuf);
          }
          else
            session->callback(ec);
        }
        else {
          if(session->connection->attempt_reconnect && ec != error::operation_aborted)
            reconnect(session, ec);
          else
            session->callback(ec);
        }
      });
    }

    void reconnect(const std::shared_ptr<Session> &session, const error_code &ec) {
      LockGuard lock(connections_mutex);
      auto it = connections.find(session->connection);
      if(it != connections.end()) {
        connections.erase(it);
        session->connection = create_connection();
        session->connection->attempt_reconnect = false;
        session->connection->in_use = true;
        session->response = std::shared_ptr<Response>(new Response(this->config.max_response_streambuf_size, session->connection));
        connections.emplace(session->connection);
        lock.unlock();
        this->connect(session);
      }
      else {
        lock.unlock();
        session->callback(ec);
      }
    }

    void read_content(const std::shared_ptr<Session> &session, std::size_t remaining_length) {
      asio::async_read(*session->connection->socket, session->response->streambuf, asio::transfer_exactly(remaining_length), [this, session, remaining_length](const error_code &ec, std::size_t bytes_transferred) {
        auto lock = session->connection->handler_runner->continue_lock();
        if(!lock)
          return;

        if(!ec) {
          if(session->response->streambuf.size() == session->response->streambuf.max_size() && remaining_length > bytes_transferred) {
            session->response->content.end = false;
            session->callback(ec);
            session->response = std::shared_ptr<Response>(new Response(*session->response));
            this->read_content(session, remaining_length - bytes_transferred);
          }
          else
            session->callback(ec);
        }
        else
          session->callback(ec);
      });
    }

    /// Ignore end of file error codes
    virtual error_code clean_error_code(const error_code &ec) {
      return ec == error::eof ? error_code() : ec;
    }

    void read_content(const std::shared_ptr<Session> &session) {
      asio::async_read(*session->connection->socket, session->response->streambuf, [this, session](const error_code &ec_, std::size_t /*bytes_transferred*/) {
        auto lock = session->connection->handler_runner->continue_lock();
        if(!lock)
          return;

        auto ec = clean_error_code(ec_);

        if(!ec) {
          {
            LockGuard lock(this->connections_mutex);
            this->connections.erase(session->connection);
          }
          if(session->response->streambuf.size() == session->response->streambuf.max_size()) {
            session->response->content.end = false;
            session->callback(ec);
            session->response = std::shared_ptr<Response>(new Response(*session->response));
            this->read_content(session);
          }
          else
            session->callback(ec);
        }
        else
          session->callback(ec);
      });
    }

    void read_chunked_transfer_encoded(const std::shared_ptr<Session> &session, const std::shared_ptr<asio::streambuf> &chunk_size_streambuf) {
      asio::async_read_until(*session->connection->socket, *chunk_size_streambuf, "\r\n", [this, session, chunk_size_streambuf](const error_code &ec, size_t bytes_transferred) {
        auto lock = session->connection->handler_runner->continue_lock();
        if(!lock)
          return;

        if(!ec) {
          std::istream istream(chunk_size_streambuf.get());
          std::string line;
          std::getline(istream, line);
          bytes_transferred -= line.size() + 1;
          unsigned long chunk_size = 0;
          try {
            chunk_size = std::stoul(line, 0, 16);
          }
          catch(...) {
            session->callback(make_error_code::make_error_code(errc::protocol_error));
            return;
          }

          if(chunk_size == 0) {
            session->callback(error_code());
            return;
          }

          if(chunk_size + session->response->streambuf.size() > session->response->streambuf.max_size()) {
            session->response->content.end = false;
            session->callback(ec);
            session->response = std::shared_ptr<Response>(new Response(*session->response));
          }

          auto num_additional_bytes = chunk_size_streambuf->size() - bytes_transferred;

          auto bytes_to_move = std::min<std::size_t>(chunk_size, num_additional_bytes);
          if(bytes_to_move > 0) {
            auto &source = *chunk_size_streambuf;
            auto &target = session->response->streambuf;
            target.commit(asio::buffer_copy(target.prepare(bytes_to_move), source.data(), bytes_to_move));
            source.consume(bytes_to_move);
          }

          if(chunk_size > num_additional_bytes) {
            asio::async_read(*session->connection->socket, session->response->streambuf, asio::transfer_exactly(chunk_size - num_additional_bytes), [this, session, chunk_size_streambuf](const error_code &ec, size_t /*bytes_transferred*/) {
              auto lock = session->connection->handler_runner->continue_lock();
              if(!lock)
                return;

              if(!ec) {
                // Remove "\r\n"
                auto null_buffer = std::make_shared<asio::streambuf>(2);
                asio::async_read(*session->connection->socket, *null_buffer, asio::transfer_exactly(2), [this, session, chunk_size_streambuf, null_buffer](const error_code &ec, size_t /*bytes_transferred*/) {
                  auto lock = session->connection->handler_runner->continue_lock();
                  if(!lock)
                    return;
                  if(!ec)
                    read_chunked_transfer_encoded(session, chunk_size_streambuf);
                  else
                    session->callback(ec);
                });
              }
              else
                session->callback(ec);
            });
          }
          else if(2 + chunk_size > num_additional_bytes) { // If only end of chunk remains unread (\n or \r\n)
            // Remove "\r\n"
            if(2 + chunk_size - num_additional_bytes == 1)
              istream.get();
            auto null_buffer = std::make_shared<asio::streambuf>(2);
            asio::async_read(*session->connection->socket, *null_buffer, asio::transfer_exactly(2 + chunk_size - num_additional_bytes), [this, session, chunk_size_streambuf, null_buffer](const error_code &ec, size_t /*bytes_transferred*/) {
              auto lock = session->connection->handler_runner->continue_lock();
              if(!lock)
                return;
              if(!ec)
                read_chunked_transfer_encoded(session, chunk_size_streambuf);
              else
                session->callback(ec);
            });
          }
          else {
            // Remove "\r\n"
            istream.get();
            istream.get();

            read_chunked_transfer_encoded(session, chunk_size_streambuf);
          }
        }
        else
          session->callback(ec);
      });
    }

    void read_server_sent_event(const std::shared_ptr<Session> &session, const std::shared_ptr<asio::streambuf> &events_streambuf) {
      asio::async_read_until(*session->connection->socket, *events_streambuf, HeaderEndMatch(), [this, session, events_streambuf](const error_code &ec, std::size_t /*bytes_transferred*/) {
        auto lock = session->connection->handler_runner->continue_lock();
        if(!lock)
          return;

        if(!ec) {
          session->response->content.end = false;
          std::istream istream(events_streambuf.get());
          std::ostream ostream(&session->response->streambuf);
          std::string line;
          while(std::getline(istream, line) && !line.empty() && !(line.back() == '\r' && line.size() == 1)) {
            ostream.write(line.data(), static_cast<std::streamsize>(line.size() - (line.back() == '\r' ? 1 : 0)));
            ostream.put('\n');
          }

          session->callback(ec);
          session->response = std::shared_ptr<Response>(new Response(*session->response));
          read_server_sent_event(session, events_streambuf);
        }
        else
          session->callback(ec);
      });
    }
  };

  template <class socket_type>
  class Client : public ClientBase<socket_type> {};

  using HTTP = asio::ip::tcp::socket;

  template <>
  class Client<HTTP> : public ClientBase<HTTP> {
  public:
    /**
     * Constructs a client object.
     *
     * @param server_port_path Server resource given by host[:port][/path]
     */
    Client(const std::string &server_port_path) noexcept : ClientBase<HTTP>::ClientBase(server_port_path, 80) {}

  protected:
    std::shared_ptr<Connection> create_connection() noexcept override {
      return std::make_shared<Connection>(handler_runner, *io_service);
    }

    void connect(const std::shared_ptr<Session> &session) override {
      if(!session->connection->socket->lowest_layer().is_open()) {
        auto resolver = std::make_shared<asio::ip::tcp::resolver>(*io_service);
        session->connection->set_timeout(config.timeout_connect);
        async_resolve(*resolver, *host_port, [this, session, resolver](const error_code &ec, resolver_results results) {
          session->connection->cancel_timeout();
          auto lock = session->connection->handler_runner->continue_lock();
          if(!lock)
            return;
          if(!ec) {
            session->connection->set_timeout(config.timeout_connect);
            asio::async_connect(*session->connection->socket, results, [this, session, resolver](const error_code &ec, async_connect_endpoint /*endpoint*/) {
              session->connection->cancel_timeout();
              auto lock = session->connection->handler_runner->continue_lock();
              if(!lock)
                return;
              if(!ec) {
                asio::ip::tcp::no_delay option(true);
                error_code ec;
                session->connection->socket->set_option(option, ec);
                this->write(session);
              }
              else
                session->callback(ec);
            });
          }
          else
            session->callback(ec);
        });
      }
      else
        write(session);
    }
  };
} // namespace SimpleWeb

#endif /* SIMPLE_WEB_CLIENT_HTTP_HPP */
