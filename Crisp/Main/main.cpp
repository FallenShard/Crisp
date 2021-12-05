#include <Crisp/Core/ApplicationEnvironment.hpp>
#include <Crisp/Core/Application.hpp>

int main(int argc, char** argv)
{
    crisp::ApplicationEnvironment environment(argc, argv);
    crisp::Application application(environment);
    application.run();
    return EXIT_SUCCESS;
}