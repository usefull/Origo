#pragma once

#include <string_view>

using namespace std;

namespace origo::ErrorMessages
{
    inline constexpr string_view CantOpenConfigFile = "Unable to open configuration file {}";
    inline constexpr string_view ConfigFileReadingError = "Configuration file reading error: {}";
    inline constexpr string_view CantReadIpFromConfig = "Unable to read IP-address from configuration file  (field \"ip\"):  {}";
    inline constexpr string_view CantReadPortFromConfig = "Unable to read port number from configuration file  (field \"port\"):  {}";
    inline constexpr string_view CantReadStaticDirsFromConfig = "Unable to read static files directory list from configuration file  (field \"staticDirs\"):  {}";
}

namespace origo::InfoMessages
{
    inline constexpr string_view StartPrompt = "Origo({}) started on port {}";
    inline constexpr string_view CtrlC = "Press Ctrl+C to stop gracefully...";
    inline constexpr string_view StopSigRecivied = "\nReceived stop signal. Shutting down gracefully...";
    inline constexpr string_view StoppedSuccessfully = "Origo stopped successfully.";
    inline constexpr string_view FatalError = "Fatal error: ";
}