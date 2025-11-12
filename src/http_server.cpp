// http_server.cpp
#include "http_server.h"

#include <boost/asio/dispatch.hpp>
#include <iostream>

namespace http_server {

using namespace std::literals;

void ReportError(beast::error_code ec, std::string_view what) {
    std::cerr << what << ": "sv << ec.message() << std::endl;
    value custom_data{
        {"code"sv, ec.value()},
        {"text"sv, ec.message()},
        {"where"sv, what}
    };
    BOOST_LOG_TRIVIAL(info)
        << logging::add_value(my_logger::additional_data, custom_data)
        << "error"sv;
}

void SessionBase::Run() {
// Вызываем метод Read, используя executor объекта stream_.
// Таким образом вся работа со stream_ будет выполняться, используя его executor
    net::dispatch(
        stream_.get_executor(),
        beast::bind_front_handler(&SessionBase::Read, GetSharedThis()));
}

tcp::socket& SessionBase::GetSocket() {
    return stream_.socket();
}

SessionBase::SessionBase(tcp::socket&& socket)
    : stream_(std::move(socket)) {
}

void SessionBase::Write(http_handler::ServerResponse&& response) {
    // Развернём вариант и выберем соответствующий тип тела
    std::visit([this](auto &&arg) mutable {
        using T = std::decay_t<decltype(arg)>;
        
        // Получаем тело ответа и передаём в специальную функцию записи
        DoWrite(this, arg);
    }, std::move(response));
}

void SessionBase::Read() {
    // Очищаем запрос от прежнего значения (метод Read может быть вызван несколько раз)
    request_ = {};
    stream_.expires_after(30s);
    // Считываем request_ из stream_, используя buffer_ для хранения считанных данных
    http::async_read(
        stream_,
        buffer_,
        request_,
        // По окончании операции будет вызван метод OnRead
        beast::bind_front_handler(&SessionBase::OnRead, GetSharedThis()));
}

void SessionBase::OnRead(
    beast::error_code ec,
    [[maybe_unused]] std::size_t bytes_read)
{
    if (ec == http::error::end_of_stream) {
        // Нормальная ситуация - клиент закрыл соединение
        return Close();
    }
    if (ec) {
        return ReportError(ec, "read"sv);
    }
    HandleRequest(std::move(request_), stream_.socket().remote_endpoint());
}

void SessionBase::OnWrite(
    bool close,
    beast::error_code ec,
    [[maybe_unused]] std::size_t bytes_written)
{
    if (ec) {
        return ReportError(ec, "write"sv);
    }
    if (close) {
        // Семантика ответа требует закрыть соединение
        return Close();
    }
    // Считываем следующий запрос
    Read();
}

void SessionBase::Close() {
    beast::error_code ec;
    stream_.socket().shutdown(tcp::socket::shutdown_send, ec);

    if(ec) {
        return ReportError(ec, "close"sv);
    }
}

}  // namespace http_server