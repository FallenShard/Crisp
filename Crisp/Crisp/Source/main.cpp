#include <iostream>
#include <memory>

#include "ApplicationEnvironment.hpp"
#include "Application.hpp"

int main(int argc, char** argv)
{
    auto environment = std::make_shared<crisp::ApplicationEnvironment>();
    
    auto application = std::make_shared<crisp::Application>();
    application->run();

    return 0;
}