#include "assert.hpp"
#include "status_code.hpp"

using namespace SimpleWeb;


int main() {
  ASSERT(status_code("") == StatusCode::unknown);
  ASSERT(status_code("Error") == StatusCode::unknown);
  ASSERT(status_code("000 Error") == StatusCode::unknown);
  ASSERT(status_code(StatusCode::unknown) == "");
  ASSERT(static_cast<int>(status_code("050 Custom")) == 50);
  ASSERT(static_cast<int>(status_code("950 Custom")) == 950);
  ASSERT(status_code("100 Continue") == StatusCode::information_continue);
  ASSERT(status_code("100 C") == StatusCode::information_continue);
  ASSERT(status_code("100") == StatusCode::information_continue);
  ASSERT(status_code(StatusCode::information_continue) == "100 Continue");
  ASSERT(status_code("200 OK") == StatusCode::success_ok);
  ASSERT(status_code(StatusCode::success_ok) == "200 OK");
  ASSERT(status_code("208 Already Reported") == StatusCode::success_already_reported);
  ASSERT(status_code(StatusCode::success_already_reported) == "208 Already Reported");
  ASSERT(status_code("308 Permanent Redirect") == StatusCode::redirection_permanent_redirect);
  ASSERT(status_code(StatusCode::redirection_permanent_redirect) == "308 Permanent Redirect");
  ASSERT(status_code("404 Not Found") == StatusCode::client_error_not_found);
  ASSERT(status_code(StatusCode::client_error_not_found) == "404 Not Found");
  ASSERT(status_code("502 Bad Gateway") == StatusCode::server_error_bad_gateway);
  ASSERT(status_code(StatusCode::server_error_bad_gateway) == "502 Bad Gateway");
  ASSERT(status_code("504 Gateway Timeout") == StatusCode::server_error_gateway_timeout);
  ASSERT(status_code(StatusCode::server_error_gateway_timeout) == "504 Gateway Timeout");
  ASSERT(status_code("511 Network Authentication Required") == StatusCode::server_error_network_authentication_required);
  ASSERT(status_code(StatusCode::server_error_network_authentication_required) == "511 Network Authentication Required");
}
