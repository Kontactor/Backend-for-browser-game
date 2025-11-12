// handlers_utils.cpp
#include "handlers_utils.h"

#include <charconv>
#include <string_view>

namespace http_handler {

void AddHeaders(
    StringResponse& response,
    const std::map<http::field, std::string>& headers)
{
    for(auto &[key, val]: headers) {
        response.set(key, val);
    }
}

StringResponse MakeStringResponse(
    http::status status,
    std::string_view body,
    unsigned http_version,
    bool keep_alive,
    std::string_view content_type,
    const std::map<http::field, std::string>& extra_headers)
{
    StringResponse response(status, http_version);

    response.set(http::field::content_type, content_type);
    response.body() = body;
    response.content_length(body.size());
    response.keep_alive(keep_alive);

    AddHeaders(response, extra_headers);

    return response;
}

std::string UrlDecode(const std::string& encoded) {
    std::string result;
    result.reserve(encoded.size());

    for (size_t pos = 0; pos < encoded.length(); ++pos) {
        if (encoded[pos] == '%') {
            if (pos + 2 >= encoded.length()) {
                throw std::invalid_argument("Incomplete %-sequence");
            }

            if (!std::isxdigit(encoded[pos + 1]) ||
                !std::isxdigit(encoded[pos + 2]))
            {
                throw std::invalid_argument("Invalid hex digits in %-sequence");
            }

            unsigned char byte;
            auto [ptr, ec] = std::from_chars(
                encoded.data() + pos + 1,
                encoded.data() + pos + 3,
                byte,
                16
            );

            if (ec != std::errc()) {
                throw std::invalid_argument("Failed to parse hex value");
            }

            result += static_cast<char>(byte);
            pos += 2;
        } else if (encoded[pos] == '+') {
            result += ' ';
        } else {
            result += encoded[pos];
        }
    }

    return result;
}

} // namespace http_handler