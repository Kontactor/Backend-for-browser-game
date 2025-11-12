// request_handler.h
#pragma once

#include "http_server.h"
#include "model.h"
#include "my_logger.h"
#include "api_handler.h"
#include "handlers_utils.h"

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/beast/http/file_body.hpp>
#include <boost/json.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/manipulators/add_value.hpp>

#include <iostream>
#include <filesystem>
#include <string>
#include <variant>

namespace http_handler {

constexpr char API_PATH[] = "/api/";

using namespace boost::asio::ip;
using namespace boost::json;
using namespace std::literals;

namespace net = boost::asio;
namespace beast = boost::beast;
namespace sys = boost::system;
namespace fs = std::filesystem;
namespace logging = boost::log;
namespace http = beast::http;

using Strand = net::strand<net::io_context::executor_type>;
using Clock = std::chrono::steady_clock;

BOOST_LOG_ATTRIBUTE_KEYWORD(additional_data, "AdditionalData", value)
BOOST_LOG_ATTRIBUTE_KEYWORD(timestamp, "TimeStamp", boost::posix_time::ptime)

template<class SomeRequestHandler>
class LoggingRequestHandler {
public:
    explicit LoggingRequestHandler(SomeRequestHandler& handler) 
        : decorated_(handler) {}

    template <typename Body, typename Allocator, typename Send>
    void operator()(
        http::request<Body, http::basic_fields<Allocator>>&& req,
        const tcp::endpoint& remote_endpoint,
        Send&& send);

private:
    template <typename Body, typename Allocator>
    static void LogRequest(
        const http::request<Body,
        http::basic_fields<Allocator>>& req,
        const tcp::endpoint& remote_endpoint);

    static void LogResponse(
        const ServerResponse& resp,
        int64_t processing_time_ms);

    SomeRequestHandler& decorated_;
};

class RequestHandler : public std::enable_shared_from_this<RequestHandler> {
public:
    explicit RequestHandler(
        model::Game& game,
        Strand& api_strand,
        fs::path static_files_root);

    RequestHandler(const RequestHandler&) = delete;
    RequestHandler& operator=(const RequestHandler&) = delete;

    template <typename Body, typename Allocator, typename Send>
    void operator()(
        http::request<Body, http::basic_fields<Allocator>>&& req,
        Send&& send);

private:
    bool IsSubPath(fs::path path, fs::path base);
    std::string GetMimeType(const std::string& extension);

    template <typename Body, typename Allocator>
    ServerResponse ReportServerError(
        http::request<Body, http::basic_fields<Allocator>>&& req) const;

    template <typename Body, typename Allocator>
    ServerResponse FileRequestProcessing(
        http::request<Body, http::basic_fields<Allocator>>&& req);

    model::Game& game_;
    fs::path static_files_root_;
    Strand& api_strand_;
    ApiRequestHandler api_handler_;
};

template<class SomeRequestHandler>
template <typename Body, typename Allocator, typename Send>
void LoggingRequestHandler<SomeRequestHandler>::operator()(
    http::request<Body, http::basic_fields<Allocator>>&& req,
    const tcp::endpoint& remote_endpoint,
    Send&& send)
{
    LogRequest(std::forward<decltype(req)>(req), remote_endpoint);

    const auto start = Clock::now();

    auto w_send = [send = std::move(send), start] (auto&& resp) {
        const auto end = Clock::now();
        const auto processing_time_ms = 
            std::chrono::duration_cast<std::chrono::milliseconds>(
                end - start
            ).count();

        LogResponse(resp, processing_time_ms);
        send(std::forward<decltype(resp)>(resp));
    };

    decorated_(
        std::forward<decltype(req)>(req),
        std::forward<decltype(w_send)>(w_send));
}

template<class SomeRequestHandler>
template <typename Body, typename Allocator>
void LoggingRequestHandler<SomeRequestHandler>::LogRequest(
    const http::request<Body,
    http::basic_fields<Allocator>>& req,
    const tcp::endpoint& remote_endpoint)
{
    value custom_data{
        {"ip", remote_endpoint.address().to_string()},
        {"URI", req.target()},
        {"method", req.method_string()}
    };
    BOOST_LOG_TRIVIAL(info)
        << logging::add_value(my_logger::additional_data, custom_data)
        << "request received"sv;
}

template<class SomeRequestHandler>
void LoggingRequestHandler<SomeRequestHandler>::LogResponse(
    const ServerResponse& resp,
    int64_t processing_time_ms)
{
    std::string content_type{"null"};
    int code = 0;

    std::visit([&](const auto& response) {
        using RespT = std::decay_t<decltype(response)>;
        auto it = response.base().find(http::field::content_type);
        if (it != response.base().end()) {
            content_type = std::string(it->value());
        }
        code = response.result_int();
    }, resp);

    value custom_data{
        {"response_time"s, processing_time_ms},
        {"code", code},
        {"content_type", content_type}
    };

    BOOST_LOG_TRIVIAL(info)
        << logging::add_value(my_logger::additional_data, custom_data)
        << "response sent"sv;
}

template <typename Body, typename Allocator, typename Send>
void RequestHandler::operator()(
    http::request<Body, http::basic_fields<Allocator>>&& req,
    Send&& send)
{
    ServerResponse resp;

    std::string target = UrlDecode(std::string(req.target()));
    auto version = req.version();
    auto keep_alive = req.keep_alive();

    try {
        if (target.starts_with(API_PATH)) {
            value custom_data{
                {"status"s, "start"},
                {"code", 0},
                {"content_type", "null"}
            };

            auto handle = [self = shared_from_this(), send,
                            req = std::forward<decltype(req)>(req)] () mutable {
                try {
                    // Этот assert не выстрелит, так как лямбда-функция будет выполняться внутри strand
                    assert(self->api_strand_.running_in_this_thread());
                    send(self->api_handler_.Handle(std::move(req)));
                } catch (...) {
                    send(self->ReportServerError(
                        std::forward<decltype(req)>(req)));
                }
            };
            return net::dispatch(api_strand_, handle);
        } else {
            send(FileRequestProcessing(std::forward<decltype(req)>(req)));
        }
    } catch (...) {
        send(ReportServerError(std::move(req)));
    }
}

template <typename Body, typename Allocator>
ServerResponse RequestHandler::ReportServerError(
    http::request<Body, http::basic_fields<Allocator>>&& req) const
{
    std::string error_message = "Internal server error.";

    StringResponse resp;
    resp.version(req.version());
    resp.keep_alive(req.keep_alive());
    resp.result(http::status::internal_server_error);
    resp.set(http::field::content_type, "text/plain");
    resp.body() = error_message;
    resp.content_length(error_message.size());

    return std::move(resp);
}

template <typename Body, typename Allocator>
ServerResponse RequestHandler::FileRequestProcessing(
    http::request<Body, http::basic_fields<Allocator>>&& req)
{
    std::string target = UrlDecode(std::string(req.target()));
    ServerResponse resp;
    if (req.method() == http::verb::get || req.method() == http::verb::head) {
        fs::path absolute_path = static_files_root_ / target.substr(1);

        if (!IsSubPath(absolute_path, static_files_root_)) {
                resp = std::move(MakeStringResponse(
                    http::status::bad_request,
                    "Invalid path outside of the static files directory.",
                    req.version(),
                    req.keep_alive(),
                    ContentType::TEXT_PLAIN));
        } else {
            if (fs::exists(absolute_path)) {
                if (fs::is_directory(absolute_path)) {
                    fs::path index_html = absolute_path / "index.html";
                    if (fs::exists(index_html)) {
                        absolute_path = index_html;
                    } else {
                        resp = std::move(MakeStringResponse(
                            http::status::not_found,
                            "Directory does not contain an index.html file.",
                            req.version(),
                            req.keep_alive(),
                            ContentType::TEXT_PLAIN));
                        return resp;
                    }
                }

                boost::beast::http::file_body::value_type file;

                if (sys::error_code ec;
                    file.open(
                        absolute_path.string().c_str(),
                        beast::file_mode::read, ec),
                    ec)
                {
                    resp = std::move(MakeStringResponse(
                        http::status::internal_server_error,
                        "Failed to read requested file.",
                        req.version(),
                        req.keep_alive(),
                        ContentType::TEXT_PLAIN));
                    return resp;
                }

                std::string mime_type =
                    GetMimeType(absolute_path.extension().string());

                FileResponse file_resp;
                file_resp.version(11);  // HTTP/1.1
                file_resp.result(http::status::ok);
                file_resp.insert(http::field::content_type, mime_type);
                file_resp.body() = std::move(file);
                file_resp.prepare_payload();

                resp = std::move(file_resp);

            } else {
                resp = std::move(MakeStringResponse(
                    http::status::not_found,
                    "File not found.",
                    req.version(),
                    req.keep_alive(),
                    ContentType::TEXT_PLAIN));
            }
        }
    } else {
        resp = std::move(MakeStringResponse(
            http::status::method_not_allowed,
            R"({"code": "methodNotAllowed","message": "Method Not Allowed"})",
            req.version(),
            req.keep_alive(),
            ContentType::APP_JSON));
    }
    return resp;
}

}  // namespace http_handler