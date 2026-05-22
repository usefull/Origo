#pragma once

#include "IHandler.hpp"
#include "../config.hpp"
#include "../tail_path_p.hpp"
#include "../get_mime_type.hpp"

namespace epr = restinio::router::easy_parser_router;

namespace origo {

    class StaticFilesHandler : public IHandler<StaticFilesHandler> {
    public:
        static void RegisterRoutes(DI& di, restinio::router::easy_parser_router_t& router) {
            auto config = di.resolve<Config>();
            for (const auto& [virtualDir, realDir] : config->staticDirs) {
                router.http_get(
                    epr::path_to_params(
                        fmt::format("/{}/", virtualDir),
                        tail_path_p()
                    ),
                    [&di, realDir](const auto& req, std::string pathTail) { return getHandler(di)->handle(req, realDir, pathTail); }
                );
            }
        }

        restinio::request_handling_status_t handle(
            const std::shared_ptr<restinio::generic_request_t<restinio::no_extra_data_factory_t::data_t>>& req,
            std::filesystem::path realDir,std::string pathTail)
        {
            auto filePath = realDir / pathTail;
            if (!std::filesystem::exists(filePath)) {
                return req->create_response(restinio::status_not_found())
                        .done();
            }
            return req->create_response()
                .append_header(restinio::http_field::content_type, std::string{get_mime_type(filePath)})
                .set_body(restinio::sendfile(filePath.string()))
                .done();
        }
    };
}
