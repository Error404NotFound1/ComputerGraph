#include "core/App.h"
#include "core/AppConfig.h"

#include <iostream>

int main(int /*argc*/, char** /*argv*/)
{
    cg::AppConfig config;

    cg::App app(config);
    return app.run();
}

