#include <cstdlib>

#include <Crisp/Core/Application.hpp>
#include <Crisp/Core/ApplicationEnvironment.hpp>

int main(int argc, char** argv) {
    CRISP_MAKE_LOGGER_ST("Main");
    {
        auto configParams = crisp::parseConfig(argc, argv).unwrap();
        crisp::ApplicationEnvironment environment(std::move(configParams));
        crisp::Application application(environment);
        application.run();
        CRISP_LOGI("Application shut down successfully.");
    }
    return EXIT_SUCCESS;
}
