#pragma once

#include <iostream>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <ctime>

#include "IHandler.hpp"
#include "CorsHandler.hpp"
#include "../config.hpp"

namespace epr = restinio::router::easy_parser_router;

namespace origo {

    class TestHandler : public IHandler<TestHandler, std::shared_ptr<Config>> {
    public:
        static void RegisterRoutes(DI& di, restinio::router::easy_parser_router_t& router) {
            router.http_get(
                epr::path_to_params(
                    "/servertime"
                ),
                [&di](const auto& req) { return getHandler(di)->get_server_time(req); }
            );
        }

        TestHandler(std::shared_ptr<origo::Config> config) : m_config(config) {}

        restinio::request_handling_status_t get_server_time(const std::shared_ptr<restinio::generic_request_t<restinio::no_extra_data_factory_t::data_t>>& req) {
            auto now = std::chrono::system_clock::now();
            std::time_t time_now = std::chrono::system_clock::to_time_t(now);
            std::tm tm_now = *std::localtime(&time_now);
            std::ostringstream oss;
            oss << std::put_time(&tm_now, "%d.%m%Y  %H:%M:%S");
            auto resp = req->create_response()
                .set_body(oss.str());
                CorsHandler::set_cors_headers(resp);
                return resp.done();
        }

    private:
        std::shared_ptr<origo::Config> m_config;
    };
}
