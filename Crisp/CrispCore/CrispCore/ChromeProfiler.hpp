#pragma once

#include <string>
#include <sstream>
#include <iostream>
#include <fstream>
#include <mutex>
#include <iomanip>
#include <chrono>

namespace crisp
{
    class ChromeProfiler
    {
    public:
        class Scope
        {
        public:
            Scope(const char* eventName)
            {
                begin(eventName);
            }

            ~Scope()
            {
                end();
            }

        private:

        };

        static void begin(const char* message)
        {
            std::ostream& stream = getThreadStream();
            stream << "{\"pid\":" << 1 << ",\"tid\":\"" << getThreadName() << "\",\"ts\":" << getCurrentTime()
                << ",\"ph\":\"B\",\"name\":\"" << message << "\"},\n";
                
        }

        static void end()
        {
            std::ostream& stream = getThreadStream();
            stream << "{\"pid\":" << 1 << ",\"tid\":\"" << getThreadName() << "\",\"ts\":" << getCurrentTime()
                << ",\"ph\":\"E\"},\n";
        }

        static void finalize() {
            std::ofstream& stream = getFileStream();
            stream << "{" <<
                std::quoted("pid") << ":" << 1 << "," << std::quoted("tid") << ":\"" << getThreadName() << "\"," <<
                std::quoted("ts") << ":" << getCurrentTime() << "," << std::quoted("dur") << ":" << 10 << "," <<
                std::quoted("ph") << ":" << std::quoted("X") << "," << std::quoted("name") << ":" << std::quoted("RandomEvent")
                << "}\n],\n";
            stream << "\"meta_user\": \"FallenShard\"" << ","
                << "\"meta_cpu_count\": \"12\"" << "}";
            stream.close();
        }

        static void setThreadName(const std::string& name)
        {
            getThreadName() = name;
        }

        static void flushThreadBuffer()
        {
            getFileStream() << getThreadStream().str();
            getThreadStream().str("");
        }

    private:
        static std::ofstream createFileStream()
        {
            std::ofstream fileStream("profile.json");
            fileStream << "{";
            fileStream << std::quoted("traceEvents") << ":";
            fileStream << "[\n";
            return fileStream;
        }

        static std::ofstream& getFileStream()
        {
            static std::ofstream fileStream = createFileStream();
            return fileStream;
        }

        static std::size_t getCurrentTime()
        {
            static auto timePoint = std::chrono::high_resolution_clock::now();
            return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - timePoint).count();
        }

        static std::string& getThreadName()
        {
            static thread_local std::string threadName;
            return threadName;
        }

        static std::ostringstream& getThreadStream()
        {
            static thread_local std::ostringstream stream;
            return stream;
        }
    };
}