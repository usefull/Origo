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
                [&di](const auto& req, std::uint64_t id, const std::string& title) { return getHandler(di)->test(req, id, title); }
            );
        }

        restinio::request_handling_status_t test(const std::shared_ptr<restinio::generic_request_t<restinio::no_extra_data_factory_t::data_t>>& req, std::uint64_t id, const std::string& title) {
            std::cout << "StubHandler.test" << std::endl;
            return req->create_response()
                .set_body(fmt::format("id: {} title: {}", id, title))
                .done();
        }
    };
}