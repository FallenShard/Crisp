#include <Crisp/Core/Application.hpp>
#include <Crisp/Core/ApplicationEnvironment.hpp>

int main(int argc, char** argv) {
    {
        crisp::ApplicationEnvironment environment(crisp::parse(argc, argv).unwrap());
        crisp::Application application(environment);
        application.run();
        spdlog::info("Application shut down successfully.");
    }
    return EXIT_SUCCESS;
}