#ifndef SIMPLE_WEB_ASIO_COMPATIBILITY_HPP
#define SIMPLE_WEB_ASIO_COMPATIBILITY_HPP

#include <memory>

#ifdef ASIO_STANDALONE
#include <asio.hpp>
#include <asio/steady_timer.hpp>
namespace SimpleWeb {
  namespace error = asio::error;
  using error_code = std::error_code;
  using errc = std::errc;
  using system_error = std::system_error;
  namespace make_error_code = std;
} // namespace SimpleWeb
#else
#include <boost/asio.hpp>
#include <boost/asio/steady_timer.hpp>
namespace SimpleWeb {
  namespace asio = boost::asio;
  namespace error = asio::error;
  using error_code = boost::system::error_code;
  namespace errc = boost::system::errc;
  using system_error = boost::system::system_error;
  namespace make_error_code = boost::system::errc;
} // namespace SimpleWeb
#endif

namespace SimpleWeb {
#if(ASIO_STANDALONE && ASIO_VERSION >= 101300) || BOOST_ASIO_VERSION >= 101300
  using io_context = asio::io_context;
  using resolver_results = asio::ip::tcp::resolver::results_type;
  using async_connect_endpoint = asio::ip::tcp::endpoint;

#if(ASIO_STANDALONE && ASIO_VERSION >= 101800) || BOOST_ASIO_VERSION >= 101800
  using strand = asio::strand<asio::any_io_executor>;
#else
  using strand = asio::strand<asio::executor>;
#endif

  template <typename handler_type>
  inline void post(io_context &context, handler_type &&handler) {
    asio::post(context, std::forward<handler_type>(handler));
  }
  inline void restart(io_context &context) noexcept {
    context.restart();
  }
  inline asio::ip::address make_address(const std::string &str) noexcept {
    return asio::ip::make_address(str);
  }
  template <typename socket_type, typename duration_type>
  inline std::unique_ptr<asio::steady_timer> make_steady_timer(socket_type &socket, std::chrono::duration<duration_type> duration) {
    return std::unique_ptr<asio::steady_timer>(new asio::steady_timer(socket.get_executor(), duration));
  }
  template <typename handler_type>
  inline void async_resolve(asio::ip::tcp::resolver &resolver, const std::pair<std::string, std::string> &host_port, handler_type &&handler) {
    resolver.async_resolve(host_port.first, host_port.second, std::forward<handler_type>(handler));
  }
  inline asio::executor_work_guard<io_context::executor_type> make_work_guard(io_context &context) {
    return asio::make_work_guard(context);
  }
  template <typename socket_type>
  inline asio::basic_socket<asio::ip::tcp>::executor_type get_executor(socket_type &socket) {
    return socket.get_executor();
  }
  template <typename execution_context, typename handler_type>
  inline asio::executor_binder<typename asio::decay<handler_type>::type, typename execution_context::executor_type> bind_executor(strand &strand, handler_type &&handler) {
    return asio::bind_executor(strand, std::forward<handler_type>(handler));
  }
#else
  using io_context = asio::io_service;
  using resolver_results = asio::ip::tcp::resolver::iterator;
  using async_connect_endpoint = asio::ip::tcp::resolver::iterator;
  using strand = asio::io_service::strand;

  template <typename handler_type>
  inline void post(io_context &context, handler_type &&handler) {
    context.post(std::forward<handler_type>(handler));
  }
  template <typename handler_type>
  inline void post(strand &strand, handler_type &&handler) {
    strand.post(std::forward<handler_type>(handler));
  }
  inline void restart(io_context &context) noexcept {
    context.reset();
  }
  inline asio::ip::address make_address(const std::string &str) noexcept {
    return asio::ip::address::from_string(str);
  }
  template <typename socket_type, typename duration_type>
  inline std::unique_ptr<asio::steady_timer> make_steady_timer(socket_type &socket, std::chrono::duration<duration_type> duration) {
    return std::unique_ptr<asio::steady_timer>(new asio::steady_timer(socket.get_io_service(), duration));
  }
  template <typename handler_type>
  inline void async_resolve(asio::ip::tcp::resolver &resolver, const std::pair<std::string, std::string> &host_port, handler_type &&handler) {
    resolver.async_resolve(asio::ip::tcp::resolver::query(host_port.first, host_port.second), std::forward<handler_type>(handler));
  }
  inline io_context::work make_work_guard(io_context &context) {
    return io_context::work(context);
  }
  template <typename socket_type>
  inline io_context &get_executor(socket_type &socket) {
    return socket.get_io_service();
  }
  template <typename handler_type>
  inline asio::detail::wrapped_handler<strand, handler_type, asio::detail::is_continuation_if_running> bind_executor(strand &strand, handler_type &&handler) {
    return strand.wrap(std::forward<handler_type>(handler));
  }
#endif
} // namespace SimpleWeb

#endif /* SIMPLE_WEB_ASIO_COMPATIBILITY_HPP */
