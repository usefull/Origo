#include "../include/di.hpp"
#include "../include/handlers/TestHandler.hpp"

void origo::DI::init(const char* configPath) {
    auto pConfig = std::make_shared<origo::Config>(origo::Config::from_file(configPath));
    registerInstance(pConfig);
}