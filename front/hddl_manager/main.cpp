#include "XLinkConnector.h"
#include <csignal>
#include <cstdlib>

using namespace hddl;

static XLinkConnector connector;

void uninitialize(int sig)
{
    connector.stop();
}

int main(int argc, char* argv[])
{
    connector.init();

    signal(SIGINT, uninitialize);

    connector.run();

    return EXIT_SUCCESS;
}
