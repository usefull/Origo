#pragma once

#include "IHandler.hpp"

namespace epr = restinio::router::easy_parser_router;

namespace origo {

    /// @brief Обработчик CORS
    class CorsHandler : public IHandler<CorsHandler> {
        public:
            /// @brief Метод регистрации обработчика
            /// @param di DI-контейнер. Не используется в данном случае: обработчик не регистрируется в DI
            /// @param router Роутер HTTP-запросов
            /// @details Метод регистрирует обработчик для всех OPTIONS-запросов (preflight),
            /// который возвращаем пустой ответ с CORS-заголовками
            static void RegisterRoutes(DI& /*di*/, restinio::router::easy_parser_router_t& router) {
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

            /// @brief Метод устанавливает для ответа на HTTP-запрос CORS-заголовки
            /// @tparam RESP 
            /// @param response Ответ на HTTP-запрос
            template<typename RESP>
            static void set_cors_headers(RESP & response) {
                response.append_header("Access-Control-Allow-Origin", "*");
                response.append_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
                response.append_header("Access-Control-Allow-Headers", "Origin, X-Requested-With, Content-Type, Accept, Authorization");
                response.append_header("Access-Control-Allow-Credentials", "true");
            }
        };
}