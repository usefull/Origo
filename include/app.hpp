#pragma once

#include <restinio/all.hpp>
#include <restinio/router/easy_parser_router.hpp>

#include "messages.hpp"
#include "di.hpp"
#include "./handlers/CorsHandler.hpp"
#include "./handlers/TestHandler.hpp"
#include "./handlers/StubHandler.hpp"
#include "./config.hpp"
#include "./RadioStation.hpp"
#include "./get_mime_type.hpp"

#ifdef NDEBUG
    #define MODE "Release"
#else
    #define MODE "Debug"
#endif

namespace mess = origo::InfoMessages;

namespace origo {
    struct my_traits : public restinio::default_traits_t {
        using request_handler_t = std::function<restinio::request_handling_status_t(restinio::request_handle_t)>;
    };

    class App {
    public:
        /// @brief Конструктор
        /// @param configPath Путь к файлу конфигурации
        App(const char* configPath) :
            di(configPath),
            router(std::make_unique<restinio::router::easy_parser_router_t>())
        {
            radio_station = std::make_unique<RadioStation>("mp3");

            // Регистрируем в роутере обработчик CORS
            CorsHandler::Register(di, *router);

            // Регистрируем кастомные обработчики
            TestHandler::Register(di, *router);
            StubHandler::Register(di, *router);

            // Обработчик 404
            router->non_matched_request_handler([](const auto& req) { 
                return req->create_response(restinio::status_not_found()).done();
            });
        }
        
        /// @brief Метод запукает работу сервера
        void start() {

            radio_station->start();

            restinio::http_server_t<my_traits> server
            {
                restinio::own_io_context(),
                restinio::server_settings_t<my_traits>{}
                    .address(getConfig()->ip)
                    .port(getConfig()->port)
                    .socket_options_setter([](auto & options) {
                        options.set_option(asio::ip::tcp::no_delay{true});
                    })
                    .request_handler([this](auto req) { 
                        return this->root_handler(std::move(req)); 
                    })
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

            radio_station->stop();
            
            std::cout << mess::StoppedSuccessfully << std::endl;
        }

    private:

        /// @brief Метод представляет корневой обработчик запросов
        /// @param req HTTP-запрос
        /// @return HTTP-статус
        restinio::request_handling_status_t root_handler(restinio::request_handle_t req) {            
            const auto url = req->header().path();

            // Перехватываем специальную точку входа для радио
            if (url == "/radio") {
                // Создаем ответ с поддержкой Chunked Transfer Encoding
                auto response = std::make_shared<RadioStation::response_t>(
                    req->create_response<restinio::chunked_output_t>()
                );

                // Задаем заголовки аудиопотока
                response->connection_keep_alive();
                response->append_header(restinio::http_field::content_type, "audio/mpeg");
                response->append_header(restinio::http_field::cache_control, "no-cache, no-store");
                
                // Отправляем заголовки (клиент начнет ожидать данные)
                response->flush();

                // Регистрируем клиента в вещателе
                radio_station->add_client(response);

                // Говорим RESTinio, что запрос обработан, но не завершен (holding)
                return restinio::request_handling_status_t::accepted;
            }

            // Выделяем из запроса первый сегмент
            const auto second_slash_pos = url.find('/', 1);    
            auto virtualDir = (second_slash_pos == std::string_view::npos)
                ? url.substr(1)
                : url.substr(1, second_slash_pos - 1);
            
            // Выделяем из запроса остаток пути после первого сегмента
            auto tailPath = (second_slash_pos == std::string_view::npos)
                ? ""
                : url.substr(second_slash_pos + 1);

            // Пытаемся найти в конфигурации в staticDirs виртуальную папку,
            // совпадающую с первым сегментом пути
            auto config = di.resolve<Config>();
            auto it = config->staticDirs.find(std::string(virtualDir));
            if (it != config->staticDirs.end()) {
                // Если найдено, формируем полный путь к запрашиваемому файлу,
                // Если остаток пути из запроса пустой - подставляем index.html
                auto filePath = it->second / (tailPath == "" ? "index.html" : tailPath);

                // 404, если файла не существует
                if (!std::filesystem::is_regular_file(filePath)) {
                    return req->create_response(restinio::status_not_found()).done();
                }

                // Возвращаем файл в ответе
                return req->create_response()
                    .append_header(restinio::http_field::content_type, std::string{get_mime_type(filePath)})
                    .set_body(restinio::sendfile(filePath.string()))
                    .done();
            }

            // Если виртуальнв=ая папка не найдена в конфигурации,
            // передаём дальнейшую обработку запроса роутеру.
            return (*router)(req);
        }

        /// @brief Метод получения конфигурационной информации
        std::shared_ptr<origo::Config> getConfig() { return di.resolve<origo::Config>(); }

        static void signal_handler(int) {
            s_stop_flag.store(true);
            s_cv.notify_one();
        }

        DI di;
        std::unique_ptr<restinio::router::easy_parser_router_t> router;
        std::unique_ptr<RadioStation> radio_station;

        static inline std::atomic<bool> s_stop_flag{false};
        static inline std::condition_variable s_cv;
        static inline std::mutex s_mutex;
    };
}