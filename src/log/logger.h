#pragma once

//-----------------------------------------------------------------------------
// Copyright (C) 2009-2023 Carlos Aragonés
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt
//-----------------------------------------------------------------------------

#include "fmt/core.h"
//--
#include <vector>
#include <cstdarg>
#include <mutex>

#define kUseMutex

#if defined(kUseMutex)
    #define lockGuard()     std::lock_guard guard(mMutex)
#else
    #define lockGuard()
#endif

//-------------------------------------
namespace MindShake {

    //---------------------------------
    enum class LogLevel {
        Debug     = 1,
        Info      = 2,
        Bold      = 3,
        Warning   = 4,
        Error     = 5,
        Exception = 6,
    };

    // Interface for concrete loggers
    //---------------------------------
    class LoggerInterface {
        public:
                    LoggerInterface();
            virtual ~LoggerInterface();

            virtual void log(LogLevel level, const char *msg) const = 0;
    };

    // Static class logger
    //---------------------------------
    class Logger {
        public:
            template <typename ... Args>
            static void debug(const char *format, Args&& ... args) {
                log(LogLevel::Debug, format, std::forward<Args>(args)...);
            }

            template <typename ... Args>
            static void info(const char *format, Args&& ... args) {
                log(LogLevel::Info, format, std::forward<Args>(args)...);
            }

            template <typename ... Args>
            static void bold(const char *format, Args&& ... args) {
                log(LogLevel::Bold, format, std::forward<Args>(args)...);
            }

            template <typename ... Args>
            static void warning(const char *format, Args&& ... args) {
                log(LogLevel::Warning, format, std::forward<Args>(args)...);
            }

            template <typename ... Args>
            static void error(const char *format, Args&& ... args) {
                log(LogLevel::Error, format, std::forward<Args>(args)...);
            }

            template <typename ... Args>
            static void exception(const char *format, Args&& ... args) {
                log(LogLevel::Exception, format, std::forward<Args>(args)...);
            }

            template <typename ... Args>
            static void log(LogLevel level, const char *format, Args&& ... args) {
                if (asInteger(level) < asInteger(mLevel))
                    return;

                std::string msg;
                try {
                    msg = fmt::format(format, std::forward<Args>(args)...);
                }
                catch (const std::exception &e) {
                    level = LogLevel::Exception;
                    msg = e.what();
                }

                const char *buffer = msg.c_str();
                {
                    lockGuard();
                    for (const auto *logger : mLoggers) {
                        logger->log(level, buffer);
                    }
                }
            }

            static const char *getLevelName(LogLevel level);

            static void setLevel(LogLevel level) { mLevel = level; }

            static void registerLogger(LoggerInterface *logger);
            static void unRegisterLogger(LoggerInterface *logger);

        protected:
            template <typename Enumeration>
            static inline auto
            asInteger(Enumeration const value) -> typename std::underlying_type<Enumeration>::type {
                return static_cast<typename std::underlying_type<Enumeration>::type>(value);
            }

        protected:
            static inline std::vector<LoggerInterface *> mLoggers;
            static inline std::mutex                     mMutex;
            static inline LogLevel                       mLevel { LogLevel::Debug };
    };

} // end of namespace