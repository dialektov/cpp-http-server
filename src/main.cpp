#include "http_server.hpp"
#include "server_config.hpp"

#include <iostream>

int main(int argc, char* argv[]) {
    try {
        const ServerConfig config = load_server_config(argc, argv);
        HttpServer server(config);
        server.run();
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        return 1;
    }

    return 0;
}
