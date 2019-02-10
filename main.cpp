#include <iostream>

#include "Logger.h"

#include <thread>
#include <vector>

using namespace gits;

void expectedUsage()
{
//    logObj << "My log line" << 1 << "\n";
//    logObj(LogSeverity::error) << "Achtung!!!\n";

//    logger.log('c');
//    logger.log(32);
//    logger.log(45.44);
//    logger.log("string");
}

void threadFunc()
{
    logObj().log("Hello there!");

    logObj(LogSeverity::error) << "Interesting log" << 11 << gits::endl;
}

void threadFunc2()
{
    static int thread_num = 0;

    for (int i = 0; i < 10; i++)
    {
        logObj() << "Hello from thread #" << ++thread_num;
        logObj() << "Iteration #" << i+1 << gits::endl;
    }
}

int main()
{
    std::cout << "Let's go!" << std::endl;


    Logger& logger = Logger::get();
    logger.init("MyLog.txt", LogOutput::everywhere);
    logger.open();
    //////////////

    logger.log("test log", LogSeverity::debug);

    std::vector<std::thread> threads;
    for (int i = 0; i < 10; i++)
    {
        threads.emplace_back(std::thread(i % 2 ? threadFunc : threadFunc2));
    }


    threadFunc();

    logger << 12345;


    for (auto& th : threads)
    {
        if (th.joinable())
        {
            th.join();
        }
    }

    ///////////////
    logger.close();

    return 0;
}
