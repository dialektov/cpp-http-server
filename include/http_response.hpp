#pragma once

#include <string>

struct HttpResponse {
    int status_code = 200;
    std::string status_text = "OK";
    std::string content_type = "text/plain; charset=utf-8";
    std::string body;
    bool keep_alive = false;
};

std::string build_http_response(const HttpResponse& response);

HttpResponse make_text_response(int status, const std::string& status_text,
                                const std::string& body,
                                const std::string& content_type = "text/plain; charset=utf-8");

HttpResponse make_json_response(int status, const std::string& status_text,
                                const std::string& json_body);
