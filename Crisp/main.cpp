#include <cstdlib>

#include <Crisp/Core/Application.hpp>
#include <Crisp/Core/ApplicationEnvironment.hpp>

int main(int argc, char** argv) {
    {
        crisp::ApplicationEnvironment environment(crisp::parse(argc, argv).unwrap());
        crisp::Application application(environment);
        application.run();
        SPDLOG_INFO("Application shut down successfully.");
    }
    return EXIT_SUCCESS;
}