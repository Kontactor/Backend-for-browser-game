// request_handler.cpp
#include "request_handler.h"

namespace http_handler {

RequestHandler::RequestHandler(
    model::Game& game,
    Strand& api_strand,
    fs::path static_files_root)
    : game_{game}
    , api_strand_{api_strand}
    , static_files_root_{static_files_root}
    , api_handler_{game} {
}

bool RequestHandler::IsSubPath(fs::path path, fs::path base) {
    // Приводим оба пути к каноничному виду (без . и ..)
    path = fs::weakly_canonical(path);
    base = fs::weakly_canonical(base);

    // Проверяем, что все компоненты base содержатся внутри path
    for (auto b = base.begin(), p = path.begin(); b != base.end(); ++b, ++p) {
        if (p == path.end() || *p != *b) {
            return false;
        }
    }
    return true;
}

std::string RequestHandler::GetMimeType(const std::string& extension) {
    std::string ext = extension.empty() ? "" : extension.substr(1);
    transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    if (ext == "htm" || ext == "html") return "text/html";
    if (ext == "css") return "text/css";
    if (ext == "txt") return "text/plain";
    if (ext == "js") return "text/javascript";
    if (ext == "json") return "application/json";
    if (ext == "xml") return "application/xml";
    if (ext == "png") return "image/png";
    if (ext == "jpg" || ext == "jpe" || ext == "jpeg") return "image/jpeg";
    if (ext == "gif") return "image/gif";
    if (ext == "bmp") return "image/bmp";
    if (ext == "ico") return "image/vnd.microsoft.icon";
    if (ext == "tiff" || ext == "tif") return "image/tiff";
    if (ext == "svg" || ext == "svgz") return "image/svg+xml";
    if (ext == "mp3") return "audio/mpeg";

    return "application/octet-stream";
}

}  // namespace http_handler