#include <iostream>
#include "GitsLogger.h"

#include <vector>

#include <chrono>
#include <ctime>

#include "LoggerUsage.h"

using namespace std;
using namespace gits;

namespace  {
    void func()
    {
        static int k = 0;
        k++;
        for (int i = 0; i < 10; i++)
        {
            gits::logObj(Logger::LogSeverity::error) << "Hello" << " from " << " thread #" << k << Logger::endl;
            gits::logObj() << "Iteration #" << i+1  << Logger::endl;
            this_thread::sleep_for(std::chrono::milliseconds(rand()%10));
        }
    }
}

int main()
{
    cout << "Let's go!" << endl;


    using namespace gits;
    using LogSeverity = Logger::LogSeverity;
    try
    {
        Logger& logger = Logger::get();
        logger.init("test.log");
        logger.open();


        std::vector<std::thread> threads;
        for (int i = 0; i < 10; i++)
        {
            threads.emplace_back(std::thread(func));
        }

        logger.log("Achtung!!!", LogSeverity::error);
        logger.log(42, LogSeverity::warning);
        logger.log('c', LogSeverity::info);

        logger.log(3.14, LogSeverity::debug);


        logger << "streamed log";
        logger << "more stream" << 123 << 'c' << Logger::endl;
        logger(LogSeverity::error) << "Error is here" << Logger::endl;

        for (auto& elem : threads)
        {
            elem.join();
        }

        usage();

        logger.close();
    }
    catch (std::runtime_error& e)
    {
        std::cout << "Exception caught: " << e.what() << std::endl;
    }
    catch (...)
    {
        std::cout << "Unknown exceptopn caught" << std::endl;
    }

    return 0;
}
