#include <iostream>
#include <memory>

#include "Core/ApplicationEnvironment.hpp"
#include "Core/Application.hpp"

int main(int argc, char** argv)
{
    auto environment = std::make_unique<crisp::ApplicationEnvironment>();
    auto application = std::make_unique<crisp::Application>(environment.get());
    application->run();

    return 0;
}