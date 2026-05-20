#pragma once

#include "IHandler.hpp"

namespace epr = restinio::router::easy_parser_router;

namespace origo {

    class StubHandler : public IHandler<StubHandler> {
    public:
        static void RegisterRoutes(DI& di, restinio::router::easy_parser_router_t& router) {
            router.http_get(
                epr::path_to_params(
                    "/api/",
                    epr::non_negative_decimal_number_p<std::uint64_t>(),
                    "/www/",
                    epr::path_fragment_p()
                ),
                [&di](const auto& req, std::uint64_t id, const std::string& title) {
                    auto const handler = getHandler(di);                 
                    return req->create_response()
                        .set_body(fmt::format("id: {}, www: {}", id, title))
                        .done();
                }
            );
        }
    };
}