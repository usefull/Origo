#pragma once

#include "IHandler.hpp"

namespace epr = restinio::router::easy_parser_router;

namespace origo {

    class TestHandler : public IHandler<TestHandler> {
    public:
        static void RegisterRoutes(DI& di, restinio::router::easy_parser_router_t& router) {
            router.http_get(
                epr::path_to_params(
                    "/api/",
                    epr::non_negative_decimal_number_p<std::uint64_t>()
                ),
                [&di](const auto& req, std::uint64_t id) { return getHandler(di)->test(req, id); }
            );
        }

        restinio::request_handling_status_t test(const std::shared_ptr<restinio::generic_request_t<restinio::no_extra_data_factory_t::data_t>>& req, std::uint64_t id) {
            cout << "qqqqqqqqqqqqqqqqq" << endl;
            return req->create_response()
                .set_body(fmt::format("id: {}", id))
                .done();
        }
    };
}
