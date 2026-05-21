#pragma once

#include <restinio/all.hpp>
#include <restinio/router/easy_parser_router.hpp>

#include "../di.hpp"

namespace origo {

    template<typename THandler, typename... Deps>
    class IHandler {
    public:
        static void Register(DI& di, restinio::router::easy_parser_router_t& router){
            di.registerType<THandler, Deps...>();
            THandler::RegisterRoutes(di, router);
        };

    protected:
        static std::shared_ptr<THandler> getHandler(DI& di) {
            auto r = di.resolve<THandler>();
            return r;
        }
    };
}