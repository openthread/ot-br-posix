#ifndef SIMPLE_WEB_SERVER_HTTPS_HPP
#define SIMPLE_WEB_SERVER_HTTPS_HPP

#include "server_http.hpp"

#ifdef ASIO_STANDALONE
#include <asio/ssl.hpp>
#else
#include <boost/asio/ssl.hpp>
#endif

#include <algorithm>
#include <openssl/ssl.h>

namespace SimpleWeb {
  using HTTPS = asio::ssl::stream<asio::ip::tcp::socket>;

  template <>
  class Server<HTTPS> : public ServerBase<HTTPS> {
    bool set_session_id_context = false;

  public:
    /**
     * Constructs a server object.
     *
     * @param certification_file Sends the given certification file to client.
     * @param private_key_file   Specifies the file containing the private key for certification_file.
     * @param verify_file        If non-empty, use this certificate authority file to perform verification of client's certificate and hostname according to RFC 2818.
     */
    Server(const std::string &certification_file, const std::string &private_key_file, const std::string &verify_file = std::string())
        : ServerBase<HTTPS>::ServerBase(443),
#if(ASIO_STANDALONE && ASIO_VERSION >= 101300) || BOOST_ASIO_VERSION >= 101300
          context(asio::ssl::context::tls_server) {
      // Disabling TLS 1.0 and 1.1 (see RFC 8996)
      context.set_options(asio::ssl::context::no_tlsv1);
      context.set_options(asio::ssl::context::no_tlsv1_1);
#else
          context(asio::ssl::context::tlsv12) {
#endif

      context.use_certificate_chain_file(certification_file);
      context.use_private_key_file(private_key_file, asio::ssl::context::pem);

      if(verify_file.size() > 0) {
        context.load_verify_file(verify_file);
        context.set_verify_mode(asio::ssl::verify_peer | asio::ssl::verify_fail_if_no_peer_cert | asio::ssl::verify_client_once);
        set_session_id_context = true;
      }
    }

  protected:
    asio::ssl::context context;

    void after_bind() override {
      if(set_session_id_context) {
        // Creating session_id_context from address:port but reversed due to small SSL_MAX_SSL_SESSION_ID_LENGTH
        auto session_id_context = std::to_string(acceptor->local_endpoint().port()) + ':';
        session_id_context.append(config.address.rbegin(), config.address.rend());
        SSL_CTX_set_session_id_context(context.native_handle(),
                                       reinterpret_cast<const unsigned char *>(session_id_context.data()),
                                       static_cast<unsigned int>(std::min<std::size_t>(session_id_context.size(), SSL_MAX_SSL_SESSION_ID_LENGTH)));
      }
    }

    void accept() override {
      auto connection = create_connection(*io_service, context);

      acceptor->async_accept(connection->socket->lowest_layer(), [this, connection](const error_code &ec) {
        auto lock = connection->handler_runner->continue_lock();
        if(!lock)
          return;

        if(ec != error::operation_aborted)
          this->accept();

        auto session = std::make_shared<Session>(config.max_request_streambuf_size, connection);

        if(!ec) {
          asio::ip::tcp::no_delay option(true);
          error_code ec;
          session->connection->socket->lowest_layer().set_option(option, ec);

          session->connection->set_timeout(config.timeout_request);
          session->connection->socket->async_handshake(asio::ssl::stream_base::server, [this, session](const error_code &ec) {
            session->connection->cancel_timeout();
            auto lock = session->connection->handler_runner->continue_lock();
            if(!lock)
              return;
            if(!ec)
              this->read(session);
            else if(this->on_error)
              this->on_error(session->request, ec);
          });
        }
        else if(this->on_error)
          this->on_error(session->request, ec);
      });
    }
  };
} // namespace SimpleWeb

#endif /* SIMPLE_WEB_SERVER_HTTPS_HPP */
