#pragma once

//-----------------------------------------------------------------------------
// Copyright (C) 2009-2023 Carlos Aragonés
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt
//-----------------------------------------------------------------------------

#include "logger.h"
//--
#include <cstdio>

//-------------------------------------
namespace MindShake {

    // Simple console logger
    //---------------------------------
    class LoggerConsole : LoggerInterface {
        public:
            void log(LogLevel level, const char *msg) const override {
                if (level == LogLevel::Debug || level == LogLevel::Info || level == LogLevel::Bold) {
                    fmt::print(stdout, "{}\n", msg);
                }
                else {
                    fmt::print(stderr, "{}\n", msg);
                }
            }
    };

} // end of namespace