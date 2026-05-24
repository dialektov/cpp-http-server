#include "middleware.hpp"

void MiddlewareChain::use(Middleware middleware) {
    middlewares_.push_back(std::move(middleware));
}

std::optional<HttpResponse> MiddlewareChain::run(const HttpRequest& request) const {
    for (const Middleware& middleware : middlewares_) {
        if (auto response = middleware(request)) {
            return response;
        }
    }
    return std::nullopt;
}
