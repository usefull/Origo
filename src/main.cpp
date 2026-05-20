#include "../include/app.hpp"

int main(int argc, char* argv[]) {
    try {
        origo::App app(argc > 1 ? argv[1] : "origo.conf");
        app.start();
        return 0;
    }
    catch(const exception& e)
    {
        cerr << origo::InfoMessages::FatalError << e.what() << endl;
        return EXIT_FAILURE;
    }
}