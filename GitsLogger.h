#pragma once

#include <string>
#include <iomanip>
#include <iostream>
#include <fstream>
#include <sstream>
#include <map>
#include <ctime>
#include <thread>
#include <mutex>
#include <utility>
#include <ctime>
#include <chrono>

#ifdef __SUPPORTS_GITS_SERIALIZER__
#include "gitsSDK/Utils/SfinaeSerializableUtils.h"
#include "gitsSDK/serializer/TSerializer.h"
#endif

#define GITS_LOG(line)   gits::logObj(gits::Logger::LogSeverity::info) << line
#define GITS_LOG_E(line) gits::logObj(gits::Logger::LogSeverity::error) << line << gits::Logger::endl;
#define GITS_LOG_W(line) gits::logObj(gits::Logger::LogSeverity::warning) << line << gits::Logger::endl;
#define GITS_LOG_I(line) gits::logObj(gits::Logger::LogSeverity::info) << line << gits::Logger::endl;
#define GITS_LOG_D(line) gits::logObj(gits::Logger::LogSeverity::debug) << line << gits::Logger::endl;
#define GITS_LOG_T(line) gits::logObj(gits::Logger::LogSeverity::trace) << line << gits::Logger::endl;

namespace gits
{

class Logger final
{
public: // aliases
    using GetTimestampCallback = std::function<std::string (void)>;
    using GetThreadIdCallback = std::function<std::string (void)>;
    using BlockWrapper = std::pair<std::string /*prefix*/, std::string /*sufix*/>;

    enum class LogSeverity
    {
        debug,
        trace,
        info,
        warning,
        error,
    };

    enum class LogOutput
    {
        file,
        console,
        everywhere,
    };

    inline std::string stringify(const LogSeverity severity)
    {
        std::string result = "NONE";
        const static std::map<LogSeverity, std::string> S_SEVERITY_STRINGS
        {
            { LogSeverity::error, "ERROR"},
            { LogSeverity::warning, "WARNING"},
            { LogSeverity::info, "INFO"},
            { LogSeverity::debug, "DEBUG"},
            { LogSeverity::trace, "TRACE"},
        };

        auto it = S_SEVERITY_STRINGS.find(severity);
        if (it != S_SEVERITY_STRINGS.end())
        {
            result = it->second;
        }
        return result;
    }
    static constexpr auto endl = "\n";

private:
    std::mutex m_mutex;
    std::mutex m_builderMutex;

    std::string m_filename;
    std::fstream m_file;

    std::map<std::string /* thread_id */, std::stringstream /* line */> m_threadsLinesMap;
    std::map<std::string /* thread_id */, LogSeverity /* severity */> m_threadsSeveritiesMap;

    LogSeverity m_severity = LogSeverity::debug;
    LogOutput m_output = LogOutput::everywhere;

    BlockWrapper m_blockWrapper = {"[", "]"};
    GetTimestampCallback m_getTimestampClb;
    GetThreadIdCallback m_getThreadIdClb;

    bool m_isOpened = false;

private:
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    Logger()
    {
        setTimestampClb(Logger::timestamp);
        setThreadIdClb(Logger::threadId);
    }

    ~Logger()
    {
        close();
    }

public: // static
    static std::string timestamp()
    {
        using namespace std::chrono;

        auto now = system_clock::now();
        auto tt = system_clock::to_time_t ( now );

        auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;

        std::stringstream ss;
        ss << std::put_time(localtime(&tt), "%F %T");
        ss << '.' << std::setfill('0') << std::setw(3) << ms.count();

        return ss.str();
    }

    static std::string threadId()
    {
        std::stringstream ss;
        ss << std::dec << std::this_thread::get_id();
        return ss.str();
    }

    static std::string wrapValue(const std::string& value,
                                 const BlockWrapper& blockWrapper)
    {
        return blockWrapper.first + value + blockWrapper.second;
    }

public:
    static Logger& get()
    {
        static Logger self;
        return self;
    }
    void setTimestampClb(GetTimestampCallback clb)
    {
        if (m_isOpened)
        {
            return;
        }
        if (clb)
        {
            m_getTimestampClb = clb;
        }
    }
    void setThreadIdClb(GetThreadIdCallback clb)
    {
        if (m_isOpened)
        {
            return;
        }
        if (clb)
        {
            m_getThreadIdClb = clb;
        }
    }
    void setBlockWrapper(const BlockWrapper& blockWrapper)
    {
        if (m_isOpened)
        {
            return;
        }

        m_blockWrapper = blockWrapper;
    }
    void init(const std::string& filename, const LogOutput output = LogOutput::everywhere, const LogSeverity severity = LogSeverity::debug)
    {
        if (m_isOpened)
        {
            return;
        }

        m_filename = filename;
        m_output = output;
        m_severity = severity;
    }
    void flush()
    {
        if (!m_isOpened)
        {
            return;
        }
        {
            std::unique_lock<std::mutex> locker(m_builderMutex);
            for (auto& elem : m_threadsLinesMap)
            {
                log(elem.second.rdbuf(), m_threadsSeveritiesMap[elem.first]);
            }
            m_threadsLinesMap.clear();
            m_threadsSeveritiesMap.clear();
        }

        m_file.flush();
    }
    void open()
    {
        if (m_isOpened)
        {
            return;
        }

        m_file.open(m_filename, std::ios::out);
        m_isOpened = m_file.is_open();

        if (!m_isOpened)
        {
            throw std::runtime_error("Coudn't open a file " + m_filename);
        }
    }
    void close()
    {
        if (m_isOpened)
        {
            m_file.flush();
            m_file.close();
        }
        m_isOpened = false;
    }

    void log(const std::string& str, LogSeverity severity)
    {
        if (!m_isOpened)
        {
            return;
        }

        if (severity < m_severity)
        {
            return;
        }

        std::string res = wrapValue(m_getTimestampClb(), m_blockWrapper) + " "
                + wrapValue(m_getThreadIdClb(), m_blockWrapper) + " "
                + wrapValue(stringify(severity), m_blockWrapper) + " "
                + str;

        std::unique_lock<std::mutex> locker(m_mutex);
        if (m_output == LogOutput::file)
        {
            m_file << res << std::endl;
        }
        else if (m_output == LogOutput::console)
        {
            if (severity == LogSeverity::error || severity == LogSeverity::warning)
                std::cerr << res << std::endl;
            else
                std::cout << res << std::endl;
        }
        else
        {
            std::cout << res << std::endl;
            m_file << res << std::endl;
        }
    }

    template<class T>
    void log(const T& value, LogSeverity severity)
    {
        std::stringstream ss;

#ifdef __SUPPORTS_GITS_SERIALIZER__
        if constexpr (sfinae_utils::is_serializable_struct<T>::value)
        {
            serializer::Serializer<serializer::StdOstream>::serialize(ss, value);
        }
        else
        {
            ss << value;
        }
#else
        ss << value;
#endif
        log(ss.str(), severity);
    }

    template <class T>
    void addPart(const T& t)
    {
        std::unique_lock<std::mutex> locker(m_builderMutex);
        auto& ss = m_threadsLinesMap[m_getThreadIdClb()];

#ifdef __SUPPORTS_GITS_SERIALIZER__
        if constexpr (sfinae_utils::is_serializable_struct<T>::value)
        {
            serializer::Serializer<serializer::StdOstream>::serialize(ss, t);
        }
        else
        {
            ss << t;
        }
#else
    ss << t;
#endif
    }

    void flushPart()
    {
        const std::string threadId = m_getThreadIdClb();

        std::unique_lock<std::mutex> locker(m_builderMutex);
        log(m_threadsLinesMap[threadId].rdbuf(), m_threadsSeveritiesMap[threadId]);

        // cleanup map to avoid garbage (in case each thread lifetime is very short)
        m_threadsLinesMap.erase(threadId);
        m_threadsSeveritiesMap.erase(threadId);
    }

    Logger& operator()(LogSeverity severity)
    {
        if (m_threadsSeveritiesMap[m_getThreadIdClb()] != severity)
            flush();

        m_threadsSeveritiesMap[m_getThreadIdClb()] = severity;
        return get();
    }

};

inline Logger& operator<<(Logger& os, const std::string& data)
{
    if (data == Logger::endl)
    {
        os.flushPart();
    }
    else
    {
        os.addPart(data);
        auto pos = data.rfind("\n");
        if (pos != std::string::npos && pos == data.size()-1)
            os.flushPart();
    }
    return os;
}

inline Logger& operator<<(Logger& os, const char* data)
{
    os << std::string(data);
    return os;
}

template<class T>
inline Logger& operator<<(Logger& os, const T& data)
{
    os.addPart(data);
    return os;
}
inline Logger& logObj(Logger::LogSeverity severity = Logger::LogSeverity::debug)
{
    return Logger::get()(severity);
}

} // namespace gits;
