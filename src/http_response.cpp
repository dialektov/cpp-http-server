#include "http_response.hpp"

#include <sstream>

std::string build_http_response(const HttpResponse& response) {
    std::ostringstream out;
    out << "HTTP/1.1 " << response.status_code << ' ' << response.status_text << "\r\n";
    out << "Content-Type: " << response.content_type << "\r\n";
    out << "Content-Length: " << response.body.size() << "\r\n";
    out << "Connection: " << (response.keep_alive ? "keep-alive" : "close") << "\r\n";
    out << "\r\n";
    out << response.body;
    return out.str();
}

HttpResponse make_text_response(int status, const std::string& status_text,
                                const std::string& body,
                                const std::string& content_type) {
    HttpResponse response;
    response.status_code = status;
    response.status_text = status_text;
    response.content_type = content_type;
    response.body = body;
    return response;
}

HttpResponse make_json_response(int status, const std::string& status_text,
                                const std::string& json_body) {
    return make_text_response(status, status_text, json_body, "application/json; charset=utf-8");
}
