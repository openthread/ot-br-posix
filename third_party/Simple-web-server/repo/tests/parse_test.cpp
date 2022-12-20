#include "assert.hpp"
#include "client_http.hpp"
#include "server_http.hpp"
#include <iostream>

using namespace std;
using namespace SimpleWeb;

class ServerTest : public ServerBase<HTTP> {
public:
  ServerTest() : ServerBase<HTTP>::ServerBase(8080) {}

  void accept() noexcept override {}

  void parse_request_test() {
    auto session = std::make_shared<Session>(static_cast<size_t>(-1), create_connection(*io_service));

    std::ostream stream(&session->request->content.streambuf);
    stream << "GET /test/ HTTP/1.1\r\n";
    stream << "TestHeader: test\r\n";
    stream << "TestHeader2:test2\r\n";
    stream << "TestHeader3:test3a\r\n";
    stream << "TestHeader3:test3b\r\n";
    stream << "\r\n";

    ASSERT(RequestMessage::parse(session->request->content, session->request->method, session->request->path,
                                 session->request->query_string, session->request->http_version, session->request->header));

    ASSERT(session->request->method == "GET");
    ASSERT(session->request->path == "/test/");
    ASSERT(session->request->http_version == "1.1");

    ASSERT(session->request->header.size() == 4);
    auto header_it = session->request->header.find("TestHeader");
    ASSERT(header_it != session->request->header.end() && header_it->second == "test");
    header_it = session->request->header.find("TestHeader2");
    ASSERT(header_it != session->request->header.end() && header_it->second == "test2");

    header_it = session->request->header.find("testheader");
    ASSERT(header_it != session->request->header.end() && header_it->second == "test");
    header_it = session->request->header.find("testheader2");
    ASSERT(header_it != session->request->header.end() && header_it->second == "test2");

    auto range = session->request->header.equal_range("testheader3");
    auto first = range.first;
    auto second = first;
    ++second;
    ASSERT(range.first != session->request->header.end() && range.second != session->request->header.end() &&
           ((first->second == "test3a" && second->second == "test3b") ||
            (first->second == "test3b" && second->second == "test3a")));
  }
};

class ClientTest : public ClientBase<HTTP> {
public:
  ClientTest(const std::string &server_port_path) : ClientBase<HTTP>::ClientBase(server_port_path, 80) {}

  std::shared_ptr<Connection> create_connection() noexcept override {
    return nullptr;
  }

  void connect(const std::shared_ptr<Session> &) noexcept override {}

  void parse_response_header_test() {
    std::shared_ptr<Response> response(new Response(static_cast<size_t>(-1), nullptr));

    ostream stream(&response->streambuf);
    stream << "HTTP/1.1 200 OK\r\n";
    stream << "TestHeader: test\r\n";
    stream << "TestHeader2:  test2\r\n";
    stream << "TestHeader3:test3a\r\n";
    stream << "TestHeader3:test3b\r\n";
    stream << "TestHeader4:\r\n";
    stream << "TestHeader5: \r\n";
    stream << "TestHeader6:  \r\n";
    stream << "\r\n";

    ASSERT(ResponseMessage::parse(response->content, response->http_version, response->status_code, response->header));

    ASSERT(response->http_version == "1.1");
    ASSERT(response->status_code == "200 OK");

    ASSERT(response->header.size() == 7);
    auto header_it = response->header.find("TestHeader");
    ASSERT(header_it != response->header.end() && header_it->second == "test");
    header_it = response->header.find("TestHeader2");
    ASSERT(header_it != response->header.end() && header_it->second == "test2");

    header_it = response->header.find("testheader");
    ASSERT(header_it != response->header.end() && header_it->second == "test");
    header_it = response->header.find("testheader2");
    ASSERT(header_it != response->header.end() && header_it->second == "test2");

    auto range = response->header.equal_range("testheader3");
    auto first = range.first;
    auto second = first;
    ++second;
    ASSERT(range.first != response->header.end() && range.second != response->header.end() &&
           ((first->second == "test3a" && second->second == "test3b") ||
            (first->second == "test3b" && second->second == "test3a")));

    header_it = response->header.find("TestHeader4");
    ASSERT(header_it != response->header.end() && header_it->second == "");
    header_it = response->header.find("TestHeader5");
    ASSERT(header_it != response->header.end() && header_it->second == "");
    header_it = response->header.find("TestHeader6");
    ASSERT(header_it != response->header.end() && header_it->second == "");
  }
};

int main() {
  ASSERT(case_insensitive_equal("Test", "tesT"));
  ASSERT(case_insensitive_equal("tesT", "test"));
  ASSERT(!case_insensitive_equal("test", "tseT"));
  CaseInsensitiveEqual equal;
  ASSERT(equal("Test", "tesT"));
  ASSERT(equal("tesT", "test"));
  ASSERT(!equal("test", "tset"));
  CaseInsensitiveHash hash;
  ASSERT(hash("Test") == hash("tesT"));
  ASSERT(hash("tesT") == hash("test"));
  ASSERT(hash("test") != hash("tset"));

  auto percent_decoded = "testing æøå !#$&'()*+,/:;=?@[]123-._~\r\n";
  auto percent_encoded = "testing%20%C3%A6%C3%B8%C3%A5%20%21%23%24%26%27%28%29%2A%2B%2C%2F%3A%3B%3D%3F%40%5B%5D123-._~%0D%0A";
  ASSERT(Percent::encode(percent_decoded) == percent_encoded);
  ASSERT(Percent::decode(percent_encoded) == percent_decoded);
  ASSERT(Percent::decode(Percent::encode(percent_decoded)) == percent_decoded);

  SimpleWeb::CaseInsensitiveMultimap fields = {{"test1", "æøå"}, {"test2", "!#$&'()*+,/:;=?@[]"}};
  auto query_string1 = "test1=%C3%A6%C3%B8%C3%A5&test2=%21%23%24%26%27%28%29%2A%2B%2C%2F%3A%3B%3D%3F%40%5B%5D";
  auto query_string2 = "test2=%21%23%24%26%27%28%29%2A%2B%2C%2F%3A%3B%3D%3F%40%5B%5D&test1=%C3%A6%C3%B8%C3%A5";
  auto query_string_result = QueryString::create(fields);
  ASSERT(query_string_result == query_string1 || query_string_result == query_string2);
  auto fields_result1 = QueryString::parse(query_string1);
  auto fields_result2 = QueryString::parse(query_string2);
  ASSERT(fields_result1 == fields_result2 && fields_result1 == fields);

  auto serverTest = make_shared<ServerTest>();
  serverTest->io_service = std::make_shared<io_context>();

  serverTest->parse_request_test();

  {
    ClientTest clientTest("test.org");
    ASSERT(clientTest.host == "test.org");
    ASSERT(clientTest.port == 80);
    clientTest.parse_response_header_test();
  }

  {
    ClientTest clientTest("test.org:8080");
    ASSERT(clientTest.host == "test.org");
    ASSERT(clientTest.port == 8080);
  }

  {
    ClientTest clientTest("test.org:test");
    ASSERT(clientTest.host == "test.org");
    ASSERT(clientTest.port == 80);
  }

  {
    ClientTest clientTest("[::1]");
    ASSERT(clientTest.host == "::1");
    ASSERT(clientTest.port == 80);
  }

  {
    ClientTest clientTest("[::1]:8080");
    ASSERT(clientTest.host == "::1");
    ASSERT(clientTest.port == 8080);
  }


  io_context io_service;
  asio::ip::tcp::socket socket(io_service);
  SimpleWeb::Server<HTTP>::Request request(static_cast<size_t>(-1), nullptr);
  {
    request.query_string = "";
    auto queries = request.parse_query_string();
    ASSERT(queries.empty());
  }
  {
    request.query_string = "=";
    auto queries = request.parse_query_string();
    ASSERT(queries.empty());
  }
  {
    request.query_string = "=test";
    auto queries = request.parse_query_string();
    ASSERT(queries.empty());
  }
  {
    request.query_string = "a=1%202%20%203&b=3+4&c&d=æ%25ø%26å%3F";
    auto queries = request.parse_query_string();
    {
      auto range = queries.equal_range("a");
      ASSERT(range.first != range.second);
      ASSERT(range.first->second == "1 2  3");
    }
    {
      auto range = queries.equal_range("b");
      ASSERT(range.first != range.second);
      ASSERT(range.first->second == "3 4");
    }
    {
      auto range = queries.equal_range("c");
      ASSERT(range.first != range.second);
      ASSERT(range.first->second == "");
    }
    {
      auto range = queries.equal_range("d");
      ASSERT(range.first != range.second);
      ASSERT(range.first->second == "æ%ø&å?");
    }
  }

  {
    SimpleWeb::CaseInsensitiveMultimap solution;
    std::stringstream header;
    auto parsed = SimpleWeb::HttpHeader::parse(header);
    ASSERT(parsed == solution);
  }
  {
    SimpleWeb::CaseInsensitiveMultimap solution = {{"Content-Type", "application/json"}};
    std::stringstream header("Content-Type: application/json");
    auto parsed = SimpleWeb::HttpHeader::parse(header);
    ASSERT(parsed == solution);
  }
  {
    SimpleWeb::CaseInsensitiveMultimap solution = {{"Content-Type", "application/json"}};
    std::stringstream header("Content-Type: application/json\r");
    auto parsed = SimpleWeb::HttpHeader::parse(header);
    ASSERT(parsed == solution);
  }
  {
    SimpleWeb::CaseInsensitiveMultimap solution = {{"Content-Type", "application/json"}};
    std::stringstream header("Content-Type: application/json\r\n");
    auto parsed = SimpleWeb::HttpHeader::parse(header);
    ASSERT(parsed == solution);
  }

  {
    {
      SimpleWeb::CaseInsensitiveMultimap solution;
      auto parsed = SimpleWeb::HttpHeader::FieldValue::SemicolonSeparatedAttributes::parse("");
      ASSERT(parsed == solution);
    }
    {
      SimpleWeb::CaseInsensitiveMultimap solution = {{"a", ""}};
      auto parsed = SimpleWeb::HttpHeader::FieldValue::SemicolonSeparatedAttributes::parse("a");
      ASSERT(parsed == solution);
    }
    {
      SimpleWeb::CaseInsensitiveMultimap solution = {{"a", ""}, {"b", ""}};
      {
        auto parsed = SimpleWeb::HttpHeader::FieldValue::SemicolonSeparatedAttributes::parse("a; b");
        ASSERT(parsed == solution);
      }
      {
        auto parsed = SimpleWeb::HttpHeader::FieldValue::SemicolonSeparatedAttributes::parse("a;b");
        ASSERT(parsed == solution);
      }
    }
    {
      SimpleWeb::CaseInsensitiveMultimap solution = {{"a", ""}, {"b", "c"}};
      {
        auto parsed = SimpleWeb::HttpHeader::FieldValue::SemicolonSeparatedAttributes::parse("a; b=c");
        ASSERT(parsed == solution);
      }
      {
        auto parsed = SimpleWeb::HttpHeader::FieldValue::SemicolonSeparatedAttributes::parse("a;b=c");
        ASSERT(parsed == solution);
      }
    }
    {
      SimpleWeb::CaseInsensitiveMultimap solution = {{"form-data", ""}};
      auto parsed = SimpleWeb::HttpHeader::FieldValue::SemicolonSeparatedAttributes::parse("form-data");
      ASSERT(parsed == solution);
    }
    {
      SimpleWeb::CaseInsensitiveMultimap solution = {{"form-data", ""}, {"test", ""}};
      {
        auto parsed = SimpleWeb::HttpHeader::FieldValue::SemicolonSeparatedAttributes::parse("form-data; test");
        ASSERT(parsed == solution);
      }
    }
    {
      SimpleWeb::CaseInsensitiveMultimap solution = {{"form-data", ""}, {"name", "file"}};
      {
        auto parsed = SimpleWeb::HttpHeader::FieldValue::SemicolonSeparatedAttributes::parse("form-data; name=\"file\"");
        ASSERT(parsed == solution);
      }
      {
        auto parsed = SimpleWeb::HttpHeader::FieldValue::SemicolonSeparatedAttributes::parse("form-data; name=file");
        ASSERT(parsed == solution);
      }
    }
    {
      SimpleWeb::CaseInsensitiveMultimap solution = {{"form-data", ""}, {"name", "file"}, {"filename", "filename.png"}};
      {
        auto parsed = SimpleWeb::HttpHeader::FieldValue::SemicolonSeparatedAttributes::parse("form-data; name=\"file\"; filename=\"filename.png\"");
        ASSERT(parsed == solution);
      }
      {
        auto parsed = SimpleWeb::HttpHeader::FieldValue::SemicolonSeparatedAttributes::parse("form-data;name=\"file\";filename=\"filename.png\"");
        ASSERT(parsed == solution);
      }
      {
        auto parsed = SimpleWeb::HttpHeader::FieldValue::SemicolonSeparatedAttributes::parse("form-data; name=file; filename=filename.png");
        ASSERT(parsed == solution);
      }
      {
        auto parsed = SimpleWeb::HttpHeader::FieldValue::SemicolonSeparatedAttributes::parse("form-data;name=file;filename=filename.png");
        ASSERT(parsed == solution);
      }
    }
    {
      SimpleWeb::CaseInsensitiveMultimap solution = {{"form-data", ""}, {"name", "fi le"}, {"filename", "file name.png"}};
      {
        auto parsed = SimpleWeb::HttpHeader::FieldValue::SemicolonSeparatedAttributes::parse("form-data; name=\"fi le\"; filename=\"file name.png\"");
        ASSERT(parsed == solution);
      }
      {
        auto parsed = SimpleWeb::HttpHeader::FieldValue::SemicolonSeparatedAttributes::parse("form-data; name=\"fi%20le\"; filename=\"file%20name.png\"");
        ASSERT(parsed == solution);
      }
      {
        auto parsed = SimpleWeb::HttpHeader::FieldValue::SemicolonSeparatedAttributes::parse("form-data; name=fi le; filename=file name.png");
        ASSERT(parsed == solution);
      }
      {
        auto parsed = SimpleWeb::HttpHeader::FieldValue::SemicolonSeparatedAttributes::parse("form-data; name=fi%20le; filename=file%20name.png");
        ASSERT(parsed == solution);
      }
    }
  }

  ASSERT(SimpleWeb::Date::to_string(std::chrono::system_clock::now()).size() == 29);
}
