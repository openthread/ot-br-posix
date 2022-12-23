#ifndef SIMPLE_WEB_CLIENT_HTTPS_HPP
#define SIMPLE_WEB_CLIENT_HTTPS_HPP

#include "client_http.hpp"

#ifdef ASIO_STANDALONE
#include <asio/ssl.hpp>
#else
#include <boost/asio/ssl.hpp>
#endif

namespace SimpleWeb {
  using HTTPS = asio::ssl::stream<asio::ip::tcp::socket>;

  template <>
  class Client<HTTPS> : public ClientBase<HTTPS> {
  public:
    /**
     * Constructs a client object.
     *
     * @param server_port_path   Server resource given by host[:port][/path]
     * @param verify_certificate Set to true (default) to verify the server's certificate and hostname according to RFC 2818.
     * @param certification_file If non-empty, sends the given certification file to server. Requires private_key_file.
     * @param private_key_file   If non-empty, specifies the file containing the private key for certification_file. Requires certification_file.
     * @param verify_file        If non-empty, use this certificate authority file to perform verification.
     */
    Client(const std::string &server_port_path, bool verify_certificate = true, const std::string &certification_file = std::string(),
           const std::string &private_key_file = std::string(), const std::string &verify_file = std::string())
        : ClientBase<HTTPS>::ClientBase(server_port_path, 443),
#if(ASIO_STANDALONE && ASIO_VERSION >= 101300) || BOOST_ASIO_VERSION >= 101300
          context(asio::ssl::context::tls_client) {
      // Disabling TLS 1.0 and 1.1 (see RFC 8996)
      context.set_options(asio::ssl::context::no_tlsv1);
      context.set_options(asio::ssl::context::no_tlsv1_1);
#else
          context(asio::ssl::context::tlsv12) {
#endif
      if(certification_file.size() > 0 && private_key_file.size() > 0) {
        context.use_certificate_chain_file(certification_file);
        context.use_private_key_file(private_key_file, asio::ssl::context::pem);
      }

      if(verify_certificate)
        context.set_verify_callback(asio::ssl::rfc2818_verification(host));

      if(verify_file.size() > 0)
        context.load_verify_file(verify_file);
      else
        context.set_default_verify_paths();

      if(verify_certificate)
        context.set_verify_mode(asio::ssl::verify_peer);
      else
        context.set_verify_mode(asio::ssl::verify_none);
    }

  protected:
    asio::ssl::context context;

    /// Ignore for end of file and SSL_R_SHORT_READ error codes
    error_code clean_error_code(const error_code &ec) override {
      return ec == error::eof || ec == asio::ssl::error::stream_truncated ? error_code() : ec;
    }

    std::shared_ptr<Connection> create_connection() noexcept override {
      return std::make_shared<Connection>(handler_runner, *io_service, context);
    }

    void connect(const std::shared_ptr<Session> &session) override {
      if(!session->connection->socket->lowest_layer().is_open()) {
        auto resolver = std::make_shared<asio::ip::tcp::resolver>(*io_service);
        session->connection->set_timeout(this->config.timeout_connect);
        async_resolve(*resolver, *host_port, [this, session, resolver](const error_code &ec, resolver_results results) {
          session->connection->cancel_timeout();
          auto lock = session->connection->handler_runner->continue_lock();
          if(!lock)
            return;
          if(!ec) {
            session->connection->set_timeout(this->config.timeout_connect);
            asio::async_connect(session->connection->socket->lowest_layer(), results, [this, session, resolver](const error_code &ec, async_connect_endpoint /*endpoint*/) {
              session->connection->cancel_timeout();
              auto lock = session->connection->handler_runner->continue_lock();
              if(!lock)
                return;
              if(!ec) {
                asio::ip::tcp::no_delay option(true);
                error_code ec;
                session->connection->socket->lowest_layer().set_option(option, ec);

                if(!this->config.proxy_server.empty()) {
                  auto write_buffer = std::make_shared<asio::streambuf>();
                  std::ostream write_stream(write_buffer.get());
                  auto host_port = this->host + ':' + std::to_string(this->port);
                  write_stream << "CONNECT " + host_port + " HTTP/1.1\r\n"
                               << "Host: " << host_port << "\r\n\r\n";
                  session->connection->set_timeout(this->config.timeout_connect);
                  asio::async_write(session->connection->socket->next_layer(), *write_buffer, [this, session, write_buffer](const error_code &ec, std::size_t /*bytes_transferred*/) {
                    session->connection->cancel_timeout();
                    auto lock = session->connection->handler_runner->continue_lock();
                    if(!lock)
                      return;
                    if(!ec) {
                      std::shared_ptr<Response> response(new Response(this->config.max_response_streambuf_size, session->connection));
                      session->connection->set_timeout(this->config.timeout_connect);
                      asio::async_read_until(session->connection->socket->next_layer(), response->streambuf, "\r\n\r\n", [this, session, response](const error_code &ec, std::size_t /*bytes_transferred*/) {
                        session->connection->cancel_timeout();
                        auto lock = session->connection->handler_runner->continue_lock();
                        if(!lock)
                          return;
                        if(response->streambuf.size() == response->streambuf.max_size()) {
                          session->callback(make_error_code::make_error_code(errc::message_size));
                          return;
                        }

                        if(!ec) {
                          if(!ResponseMessage::parse(response->content, response->http_version, response->status_code, response->header))
                            session->callback(make_error_code::make_error_code(errc::protocol_error));
                          else {
                            if(response->status_code.compare(0, 3, "200") != 0)
                              session->callback(make_error_code::make_error_code(errc::permission_denied));
                            else
                              this->handshake(session);
                          }
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
                  this->handshake(session);
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

    void handshake(const std::shared_ptr<Session> &session) {
      SSL_set_tlsext_host_name(session->connection->socket->native_handle(), this->host.c_str());

      session->connection->set_timeout(this->config.timeout_connect);
      session->connection->socket->async_handshake(asio::ssl::stream_base::client, [this, session](const error_code &ec) {
        session->connection->cancel_timeout();
        auto lock = session->connection->handler_runner->continue_lock();
        if(!lock)
          return;
        if(!ec)
          this->write(session);
        else
          session->callback(ec);
      });
    }
  };
} // namespace SimpleWeb

#endif /* SIMPLE_WEB_CLIENT_HTTPS_HPP */
