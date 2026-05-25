#pragma once

#include <restinio/all.hpp>
#include <restinio/router/easy_parser_router.hpp>

#include "messages.hpp"
#include "di.hpp"
#include "./handlers/CorsHandler.hpp"
#include "./handlers/TestHandler.hpp"
#include "./handlers/StubHandler.hpp"
#include "./handlers/StaticFilesHandler.hpp"
#include "./config.hpp"

#ifdef NDEBUG
    #define MODE "Release"
#else
    #define MODE "Debug"
#endif

namespace mess = origo::InfoMessages;

namespace origo {
 //
    struct my_traits : public restinio::default_traits_t {
        using request_handler_t = restinio::router::easy_parser_router_t;
    };

    class App {
    public:
        App(const char* configPath) :
            di(configPath),
            router(std::make_unique<restinio::router::easy_parser_router_t>())
        {
            // Регистрируем кастомные обработчики запросов
            CorsHandler::Register(di, *router);
            TestHandler::Register(di, *router);
            StubHandler::Register(di, *router);

            // Обработчик статических файлов
            StaticFilesHandler::Register(di, *router);

            // Обработчик прочих роутов
            router->non_matched_request_handler([](const auto& req) { 
                return req->create_response(restinio::status_not_found())
                    //.set_body("Not found")
                    .done();
            });
        }

        void start() {

            restinio::http_server_t<my_traits> server
            {
                restinio::own_io_context(),
                restinio::server_settings_t<my_traits>{}
                    .address(getConfig()->ip)
                    .port(getConfig()->port)
                    .request_handler(std::move(router))
            };

            const auto cores = std::thread::hardware_concurrency();
            const auto pool_size = std::max(cores, 2u);

            restinio::on_pool_runner_t<restinio::http_server_t<my_traits>> runner
            {
                pool_size, 
                server
            };

            s_stop_flag.store(false);
            std::signal(SIGINT, signal_handler);
            std::signal(SIGTERM, signal_handler);

            runner.start();

            std::cout << fmt::format(mess::StartPrompt, MODE, getConfig()->port) << std::endl;
            std::cout << mess::CtrlC << std::endl;
            
            {
                std::unique_lock<std::mutex> lock(s_mutex);
                s_cv.wait(lock, [] { return s_stop_flag.load(); });
            }

            std::cout << mess::StopSigRecivied << std::endl;
            
            runner.stop();
            runner.wait();
            
            std::cout << mess::StoppedSuccessfully << std::endl;
        }

    private:
        std::shared_ptr<origo::Config> getConfig() { return di.resolve<origo::Config>(); }

        static void signal_handler(int) {
            s_stop_flag.store(true);
            s_cv.notify_one();
        }

        DI di;
        std::unique_ptr<restinio::router::easy_parser_router_t> router;

        static inline std::atomic<bool> s_stop_flag{false};
        static inline std::condition_variable s_cv;
        static inline std::mutex s_mutex;
    };
}
