#include <iostream>
#include "log.h"
#include <signal.h>
#include "compi.h"
#include "agentthread.h"


static void sig(int signo)
{

        switch (signo)
        {
            case SIGSEGV:
            {
                std::cout  << "Segmentation fault, aborting." << std::endl;
                exit(1);
            }
            case SIGTERM:
            case SIGINT:
	    case SIGQUIT:
            {
                if (g_bRun)
                {
                    std::cout  << "User abort" << std::endl;
                }
                g_bRun = false;
                exit(1);
            }
	    break;
        }

}

void init_signals()
{
    signal (SIGTERM, sig);
    signal (SIGINT, sig);
    signal (SIGSEGV, sig);
    signal (SIGQUIT, sig);
}


int main(int argc, char* argv[])
{
    if(argc < 2)
    {
        std::cout << "Not enought arguments. Usage: compi [config file path]" << std::endl;
        return -1;
    }


    init_signals();


    Compi app;
    return app.Run(argv[1]);

}
