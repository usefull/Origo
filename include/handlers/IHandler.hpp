#pragma once

#include <restinio/all.hpp>
#include <restinio/router/easy_parser_router.hpp>

#include "../di.hpp"

namespace origo {

    /// @brief Базовый класс для обработчиков запросов
    /// @tparam THandler 
    /// @tparam ...Deps 
    template<typename THandler, typename... Deps>
    class IHandler {
    public:
        /// @brief Метод регистрации
        /// @param di DI-контейнер
        /// @param router Роутер HTTP-запросов
        /// @details Регистрирует обработчик в DI-контейнере и выполняет регистрацию роутов,
        /// которые будет обрабатывать обработчик
        static void Register(DI& di, restinio::router::easy_parser_router_t& router){
            di.registerType<THandler, Deps...>();
            THandler::RegisterRoutes(di, router);
        };

    protected:
        /// @brief Получение экземпляра обработчика из DI-контейнера
        /// @param di DI-контейнер
        /// @return Экземпляр обработчика
        static std::shared_ptr<THandler> getHandler(DI& di) {
            auto r = di.resolve<THandler>();
            return r;
        }
    };
}