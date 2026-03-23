// Copyright (C) 2025-2026  Roman Lyubimov
// SPDX-License-Identifier: GPL-3.0-or-later
// For full license text, see <https://www.gnu.org/licenses/gpl-3.0.txt>

#include "config/config.h"
#include "webapi.h"
#include "log.h"

#include <iostream>
#include <fstream>
#include <string>
#include <cstring>
#include <algorithm>
#include <csignal>
#include <iomanip>

#ifdef __linux__
  #include <sys/sysinfo.h>
#elif __APPLE__
  #include <sys/sysctl.h>
#endif

static size_t getTotalRAM() {
#ifdef __linux__
    struct sysinfo info;
    if (sysinfo(&info) == 0) return info.totalram * info.mem_unit;
#elif __APPLE__
    int64_t mem; size_t len = sizeof(mem);
    if (sysctlbyname("hw.memsize", &mem, &len, nullptr, 0) == 0) return static_cast<size_t>(mem);
#endif
    return 0;
}

static plog::Severity parseLogLevel(const std::string& str)
{
    std::string lower = str;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c){ return std::tolower(c); });

    if (lower == "verbose") return plog::verbose;
    if (lower == "debug")   return plog::debug;
    if (lower == "info")    return plog::info;
    if (lower == "warning") return plog::warning;
    if (lower == "error")   return plog::error;
    if (lower == "fatal")   return plog::fatal;
    if (lower == "none")    return plog::none;

    std::cerr << "Unknown log level '" << str << "', falling back to 'info'" << std::endl;
    return plog::info;
}

static constexpr const char* DEFAULT_CONFIG_PATH = "/etc/put-in-pipe/config.ini";

static constexpr const char* DEFAULT_CONFIG_CONTENT = R"([server]
; Log level: verbose, debug, info, warning, error, fatal, none
log_level = info
bind_address = 0.0.0.0
bind_port = 2233

[client]
; Maximum number of simultaneous clients
max_count = 500
; Number of clients before captcha is required
without_captcha_threshold = 500
; Captcha expiration time in seconds
captcha_lifetime = 180
; Client timeout in seconds (disconnected without websocket)
timeout = 60

[session]
; Maximum number of simultaneous transfer sessions
count_limit = 100
; Maximum chunk size in bytes (default: 5 MB)
max_chunk_size = 5242880
; Maximum number of chunks in the buffer queue
chunk_queue_max_size = 10
; Session lifetime in seconds (default: 2 hours)
max_lifetime = 7200
; Maximum number of receivers per session
max_consumer_count = 5
; Initial freeze duration in seconds (time to wait for receivers)
max_initial_freeze_duration = 120
)";

static void printHelp(const char* programName)
{
    std::cout
        << "put-in-pipe — privacy-preserving streaming file transfer server\n"
        << "\n"
        << "Usage:\n"
        << "  " << programName << " [options]\n"
        << "\n"
        << "Options:\n"
        << "  -c, --config <path>       Path to config file (default: " << DEFAULT_CONFIG_PATH << ")\n"
        << "  -g, --generate-config <path>  Generate default config file at the specified path\n"
        << "  -h, --help                Show this help message\n"
        << "\n"
        << "Examples:\n"
        << "  " << programName << " -c ./config.ini\n"
        << "  " << programName << " --generate-config /etc/put-in-pipe/config.ini\n";
}

static bool generateConfig(const std::string& path)
{
    std::ifstream check(path);
    if (check.good())
    {
        std::cerr << "File already exists: " << path << std::endl;
        return false;
    }
    check.close();

    std::ofstream out(path);
    if (!out.is_open())
    {
        std::cerr << "Cannot create file: " << path << std::endl;
        return false;
    }

    out << DEFAULT_CONFIG_CONTENT;
    out.close();

    std::cout << "Config generated: " << path << std::endl;
    return true;
}

static bool matchOption(const char* arg, const char* shortOpt, const char* longOpt)
{
    return std::strcmp(arg, shortOpt) == 0 || std::strcmp(arg, longOpt) == 0;
}

int main(int argc, char* argv[])
{
    static plog::ColorConsoleAppender<plog::TxtFormatter> consoleAppender;
    plog::init(plog::debug, &consoleAppender);

    std::string configPath = DEFAULT_CONFIG_PATH;

    for (int i = 1; i < argc; ++i)
    {
        if (matchOption(argv[i], "-h", "--help"))
        {
            printHelp(argv[0]);
            return 0;
        }

        if (matchOption(argv[i], "-c", "--config"))
        {
            if (i + 1 >= argc)
            {
                std::cerr << "Missing argument for " << argv[i] << std::endl;
                return 1;
            }
            configPath = argv[++i];
            continue;
        }

        if (matchOption(argv[i], "-g", "--generate-config"))
        {
            if (i + 1 >= argc)
            {
                std::cerr << "Missing argument for " << argv[i] << std::endl;
                return 1;
            }
            return generateConfig(argv[++i]) ? 0 : 1;
        }

        std::cerr << "Unknown option: " << argv[i] << std::endl;
        printHelp(argv[0]);
        return 1;
    }

    auto& cfg = Config::instance();
    if (!cfg.loadFromFile(configPath))
    {
        std::cerr << "Failed to load config from: " << configPath << "\n"
                  << "Use --generate-config <path> to create a default config file." << std::endl;
        return 1;
    }

    plog::get()->setMaxSeverity(parseLogLevel(cfg.logLevel()));

    PLOG_INFO << "Config loaded from " << configPath;
    PLOG_INFO << "Log level: " << cfg.logLevel();
    PLOG_INFO << "Listening on " << cfg.bindAddress() << ":" << cfg.bindPort();

    {
        const size_t maxMem = cfg.transferSessionCountLimit()
                            * cfg.transferSessionMaxChunkSize()
                            * cfg.transferSessionChunkQueueMaxSize();
        const double maxMemMB = static_cast<double>(maxMem) / (1024.0 * 1024.0);
        PLOG_INFO << "Max memory for chunk buffers: "
                  << cfg.transferSessionCountLimit() << " sessions * "
                  << cfg.transferSessionMaxChunkSize() << " bytes/chunk * "
                  << cfg.transferSessionChunkQueueMaxSize() << " chunks/buffer = "
                  << std::fixed << std::setprecision(0) << maxMemMB << " MB";

        const size_t totalRAM = getTotalRAM();
        if (totalRAM > 0 && maxMem > totalRAM) {
            const double ramMB = static_cast<double>(totalRAM) / (1024.0 * 1024.0);
            PLOG_WARNING << "Max chunk buffer memory (" << maxMemMB << " MB) exceeds available RAM (" << std::fixed << std::setprecision(0) << ramMB << " MB)";
        }
    }

    WebAPI webInterface;

    auto signalHandler = [](int sig) {
        PLOG_INFO << "Received signal " << sig << ", shutting down...";
        std::_Exit(0);
    };
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    webInterface.run();

    return 0;
}
