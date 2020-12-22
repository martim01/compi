#include <iostream>
#include "log.h"
#include <signal.h>
#include "compi.h"
#include "agentthread.h"
#include <execinfo.h>
#include <unistd.h>

static void sig(int signo)
{
        switch (signo)
        {
            case SIGSEGV:
            {
                void* arr[10];
                size_t nSize = backtrace(arr, 10);

                pml::Log::Get(pml::Log::LOG_CRITICAL)  << "Segmentation fault, aborting. " << nSize << std::endl;
                for(size_t i = 0; i < nSize; i++)
                {
                    pml::Log::Get(pml::Log::LOG_CRITICAL)  << std::hex << "0x" << reinterpret_cast<int>(arr[i]) <<std::endl;
                }

                _exit(1);
            }
        case SIGTERM:
        case SIGINT:
	    case SIGQUIT:
            {
                if (g_bRun)
                {
                    pml::Log::Get(pml::Log::LOG_WARN)  << "User abort" << std::endl;
                }
                g_bRun = false;
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
