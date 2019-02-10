#pragma once

#include <map>
#include <string>
#include <iostream>
#include <sstream>
#include <fstream>
#include <iomanip>

#include <chrono>
#include <ctime>

#include <thread>
#include <mutex>

namespace gits
{

enum class LogSeverity
{
    error,
    warning,
    info,
    debug,
};

enum class LogOutput
{
    console,
    file,
    everywhere
};

std::string stringify(const LogSeverity severity)
{
    std::map<LogSeverity, std::string> S_SEVERITY_MAP =
    {
        {LogSeverity::error, "ERROR"},
        {LogSeverity::warning, "WARNING"},
        {LogSeverity::info, "INFO"},
        {LogSeverity::debug, "DEBUG"},
    };

    std::string result = "NONE";
    auto it = S_SEVERITY_MAP.find(severity);
    if (it != S_SEVERITY_MAP.end())
    {
        result = it->second;
    }
    return result;
}

const std::string endl = "\n";
/////////////////

class Logger
{
private:
    std::mutex m_logMutex;
    std::mutex m_threadChunkMutex;

private:
    static std::string timestamp()
    {
        using namespace std::chrono;

        auto now = system_clock::now();
        time_t tt = system_clock::to_time_t(now);

        auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;

        std::stringstream ss;
        ss << std::put_time(std::localtime(&tt), "%F %T");
        ss << "." << std::setfill('0') << std::setw(3) << ms.count();

        return ss.str();
    }

    static std::string threadId()
    {
        std::stringstream ss;
        ss << std::this_thread::get_id();
        return ss.str();
    }

    static std::string wrapBlock(const std::string& value, const std::string& prefix = "[", const std::string& sufix = "]")
    {
        return prefix + value + sufix;
    }

private:
    Logger()
    {

    }

    ~Logger()
    {

    }

public:
    static Logger& get()
    {
        static Logger self;
        return self;
    }

public:

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    void init(const std::string& filename,
              const LogOutput output = LogOutput::everywhere)
    {
        m_filename = filename;
        m_output = output;
    }

    void open()
    {
        if (m_isOpened)
        {
            return;
        }

        if (m_output == LogOutput::everywhere || m_output == LogOutput::file)
        {
            m_file.open(m_filename, std::ios::out);
            m_isOpened = m_file.is_open();

            if (!m_isOpened)
            {
                throw std::runtime_error("Couldn't open a log file");
            }
        }

        m_isOpened = true;
    }

    void close()
    {
        if (!m_isOpened)
        {
            return;
        }

        if (m_output == LogOutput::everywhere || m_output == LogOutput::file)
        {
            m_file.close();
        }
    }

    void flush()
    {
        if (!m_isOpened)
        {
            return;
        }

        {
            std::unique_lock<std::mutex> locker(m_threadChunkMutex);
            for (const auto& elem : m_threadLinesMap)
            {
                log(elem.second.rdbuf(), m_threadsSeverityMap[elem.first]);
            }
            m_threadLinesMap.clear();
            m_threadsSeverityMap.clear();
        }


        if (m_output == LogOutput::everywhere || m_output == LogOutput::file)
        {
            m_file.flush();
        }
    }

    void log(const std::string& msg, const LogSeverity severity = LogSeverity::debug)
    {
        std::string result = wrapBlock(timestamp()) +
                " " + wrapBlock(threadId()) +
                " " + wrapBlock(stringify(severity)) +
                " " + msg;


        std::unique_lock<std::mutex> locker(m_logMutex);
        if (m_output == LogOutput::console)
        {
            std::cout << result << std::endl;
        }
        else if (m_output == LogOutput::file)
        {
            m_file << result << std::endl;
        }
        else
        {
            std::cout << result << std::endl;
            m_file << result << std::endl;
        }
    }

    template<class T>
    void log(const T& t, const LogSeverity severity= LogSeverity::debug)
    {
        std::stringstream ss;
        ss << t;
        log(ss.str(), severity);
    }

    void addChunk(const std::string& chunk)
    {
        std::unique_lock<std::mutex> locker(m_threadChunkMutex);
        m_threadLinesMap[threadId()] << chunk;
    }

    void flushChunk()
    {
        const std::string threadID = threadId();
        std::unique_lock<std::mutex> locker(m_threadChunkMutex);
        log(m_threadLinesMap[threadID].rdbuf(), m_threadsSeverityMap[threadID]);

        // cleanup
        m_threadLinesMap.erase(threadID);
        m_threadsSeverityMap.erase(threadID);
    }


    Logger& operator()(const LogSeverity severity)
    {
        std::unique_lock<std::mutex> locker(m_threadChunkMutex);
        m_threadsSeverityMap[threadId()] = severity;
        return get();
    }

private:
    std::string m_filename;
    std::fstream m_file;
    LogOutput m_output = LogOutput::everywhere;

    std::map<std::string, LogSeverity> m_threadsSeverityMap;
    std::map<std::string, std::stringstream> m_threadLinesMap;


    bool m_isOpened = false;
};

Logger& operator<<(Logger& logger, const std::string& chunk)
{
    if (chunk == gits::endl)
    {
        logger.flushChunk();
    }
    else
    {
        logger.addChunk(chunk);
    }
    return logger;
}

template<class T>
Logger& operator<<(Logger& logger, const T& t)
{
    std::stringstream ss;
    ss << t;
    logger.addChunk(ss.str());
    return logger;
}

Logger& logObj(const LogSeverity severity = LogSeverity::debug)
{
    return Logger::get()(severity);
}

} // namespace gits
