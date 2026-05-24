#pragma once

#include "http_request.hpp"
#include "http_response.hpp"

#include <functional>
#include <optional>
#include <vector>

using Middleware = std::function<std::optional<HttpResponse>(const HttpRequest& request)>;

class MiddlewareChain {
public:
    void use(Middleware middleware);
    std::optional<HttpResponse> run(const HttpRequest& request) const;

private:
    std::vector<Middleware> middlewares_;
};
