// handlers_utils.h
#pragma once

#define BOOST_BEAST_USE_STD_STRING_VIEW

#include <boost/beast.hpp>
#include <boost/beast/http/file_body.hpp>

#include <map>
#include <variant>

namespace http_handler {

using namespace std::literals;

namespace beast = boost::beast;
namespace http = beast::http;

using StringResponse = http::response<http::string_body>;
using FileResponse = http::response<http::file_body>;
using ServerResponse = std::variant<StringResponse, FileResponse>;

struct ContentType {
    ContentType() = delete;
    constexpr static std::string_view TEXT_HTML = "text/html"sv;
    constexpr static std::string_view TEXT_PLAIN = "text/plain"sv;
    constexpr static std::string_view APP_JSON = "application/json"sv;
    // При необходимости внутрь ContentType можно добавить и другие типы контента
};

void AddHeaders(
    StringResponse& response,
    const std::map<http::field, std::string>& headers);

StringResponse MakeStringResponse(
    http::status status,
    std::string_view body,
    unsigned http_version,
    bool keep_alive,
    std::string_view content_type = ContentType::TEXT_HTML,
    const std::map<http::field, std::string>& extra_headers = {});

std::string UrlDecode(const std::string& encoded);

} // namespace http_handler