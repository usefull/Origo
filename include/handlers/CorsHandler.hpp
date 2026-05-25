#pragma once

#include "IHandler.hpp"

namespace epr = restinio::router::easy_parser_router;

namespace origo {

    class CorsHandler : public IHandler<CorsHandler> {
        public:
            static void RegisterRoutes(DI& di, restinio::router::easy_parser_router_t& router) {
                router.add_handler(
                    restinio::http_method_options(),
                    epr::path_to_params(".*"),
                    [](auto req) {
                        auto response = req->create_response();
                        set_cors_headers(response);
                        return response.done();
                    }
                );
            }

            template<typename RESP>
            static void set_cors_headers(RESP & response) {
                response.append_header("Access-Control-Allow-Origin", "*");
                response.append_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
                response.append_header("Access-Control-Allow-Headers", "Origin, X-Requested-With, Content-Type, Accept, Authorization");
                response.append_header("Access-Control-Allow-Credentials", "true");
            }
        };
}