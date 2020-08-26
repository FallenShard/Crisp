#include "Core/ApplicationEnvironment.hpp"
#include "Core/Application.hpp"

#include <iostream>

int main(int argc, char** argv)
{
    crisp::ApplicationEnvironment environment(argc, argv);
    crisp::Application application(environment);
    application.run();

    return 0;
}