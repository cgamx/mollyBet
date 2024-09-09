#pragma once

//-----------------------------------------------------------------------------
// Copyright (C) 2009-2023 Carlos Aragonés
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt
//-----------------------------------------------------------------------------

#include "logger.h"
#include <fmt/color.h>
//--
#include <cstdio>

//-------------------------------------
namespace MindShake {

    // Simple console logger
    //---------------------------------
    class LoggerColorConsole : LoggerInterface {
        public:
            void log(LogLevel level, const char *msg) const override {
                switch (level) {
                    case MindShake::LogLevel::Debug:
                        fmt::print(stdout, fg(fmt::terminal_color::bright_black), "{}\n", msg);
                        break;

                    case MindShake::LogLevel::Info:
                        fmt::print(stdout, fg(fmt::terminal_color::white), "{}\n", msg);
                        break;

                    case MindShake::LogLevel::Bold:
                        fmt::print(stdout, fg(fmt::terminal_color::green), "{}\n", msg);
                        break;

                    case MindShake::LogLevel::Warning:
                        fmt::print(stderr, fg(fmt::terminal_color::yellow), "{}\n", msg);
                        break;

                    case MindShake::LogLevel::Error:
                        fmt::print(stderr, fg(fmt::terminal_color::red), "{}\n", msg);
                        break;

                    case MindShake::LogLevel::Exception:
                        fmt::print(stderr, fg(fmt::terminal_color::red) | fmt::emphasis::underline | fmt::emphasis::bold, "{}\n", msg);
                        break;
                }
            }
    };

} // end of namespace