#pragma once

#include <chrono>
#include <fstream>
#include <sstream>
#include <string>

#include <Crisp/Core/Format.hpp>

namespace crisp {
class ChromeProfiler {
public:
    class Scope {
    public:
        Scope(const char* eventName) {
            begin(eventName);
        }

        ~Scope() {
            end();
        }
    };

    static void begin(const char* message) {
        getThreadStream() << fmt::format(
            fmt::runtime("{\"pid\":{},\"tid\":\"{}\",\"ts\":{},\"ph\":\"B\",\"name\":\"{}\"},\n"),
            1,
            getThreadName(),
            getCurrentTime(),
            message);
    }

    static void end() {
        getThreadStream() << fmt::format(
            fmt::runtime("{\"pid\":{},\"tid\":\"{}\",\"ts\":{},\"ph\":\"E\"},\n"), 1, getThreadName(), getCurrentTime());
    }

    static void finalize() {
        std::ofstream& stream = getFileStream();
        stream << "],\n";
        stream << R"("meta_user":"FallenShard","meta_cpu_count":"12"})";
        stream.close();
    }

    static void setThreadName(const std::string& name) {
        getThreadName() = name;
    }

    static void flushThreadBuffer() {
        getFileStream() << getThreadStream().str();
        getThreadStream().str("");
    }

private:
    static std::ofstream createFileStream() {
        std::ofstream stream("profile.json");
        stream << "{\"traceEvents\":[\n";
        return stream;
    }

    static std::ofstream& getFileStream() {
        static std::ofstream fileStream(createFileStream());
        return fileStream;
    }

    static std::size_t getCurrentTime() {
        static auto timePoint = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::microseconds>(
                   std::chrono::high_resolution_clock::now() - timePoint)
            .count();
    }

    static std::string& getThreadName() {
        static thread_local std::string threadName;
        return threadName;
    }

    static std::ostringstream& getThreadStream() {
        static thread_local std::ostringstream stream;
        return stream;
    }
};
} // namespace crisp