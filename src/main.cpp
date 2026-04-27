#include <restinio/all.hpp>
#include <iostream>

int main() {
    using namespace restinio;

#ifdef NDEBUG
    std::cout << "Origo is running in RELEASE mode..." << std::endl;
#else
    std::cout << "Origo is running in DEBUG mode..." << std::endl;
#endif

    std::cout << "Starting HTTP server on port 8080..." << std::endl;

    run(
        on_this_thread()
            .port(8080)
            .address("0.0.0.0")
            .request_handler([](auto req) {
                std::ostringstream json;
                json << R"({
                    "message": "Hello from Origo!",
                    "status": "ok",
                    "path": ")" << req->header().path() << R"("
                })";
                
                return req->create_response()
                    .append_header("Content-Type", "application/json")
                    .set_body(json.str())
                    .done();
            })
    );
    
    return 0;
}