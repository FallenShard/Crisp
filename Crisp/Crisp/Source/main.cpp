#include <iostream>
#include "Application.hpp"

int main(int argc, char** argv)
{
    crisp::Application::initDependencies();
    crisp::Application app;
    app.run();
    return 0;
}