#include <Crisp/Core/Application.hpp>
#include <Crisp/Core/ApplicationEnvironment.hpp>

int main(int argc, char** argv)
{
    crisp::ApplicationEnvironment environment(argc, argv);
    crisp::Application application(environment);
    application.run();
    return EXIT_SUCCESS;
}