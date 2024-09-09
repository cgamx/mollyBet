//-----------------------------------------------------------------------------
// Copyright (C) 2009-2023 Carlos Aragonés
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt
//-----------------------------------------------------------------------------

#include "logger.h"
//--
#include <cstddef>
#include <cstdio>
#include <algorithm>

#if defined(kUse_MMGR)
    #include "mmgr/mmgr.h"
#endif

//-------------------------------------
namespace MindShake {

    // Auto Register (DRY)
    //---------------------------------
    LoggerInterface::LoggerInterface() {
        Logger::registerLogger(this);
    }

    // Auto UnRegister (DRY)
    //---------------------------------
    LoggerInterface::~LoggerInterface() {
        Logger::unRegisterLogger(this);
    }

    //---------------------------------
    void
    Logger::registerLogger(LoggerInterface *logger) {
        lockGuard();
        mLoggers.emplace_back(logger);
    }

    //---------------------------------
    void
    Logger::unRegisterLogger(LoggerInterface *logger) {
        lockGuard();
        auto it = std::find(mLoggers.begin(), mLoggers.end(), logger);
        if (it != mLoggers.end()) {
            mLoggers.erase(it);
        }
    }

    //---------------------------------
    const char *
    Logger::getLevelName(LogLevel level) {
        switch (level) {
            case MindShake::LogLevel::Debug:
                return "debug";

            case MindShake::LogLevel::Info:
                return "info";

            case MindShake::LogLevel::Bold:
                return "bold";

            case MindShake::LogLevel::Warning:
                return "warning";

            case MindShake::LogLevel::Error:
                return "error";

            case MindShake::LogLevel::Exception:
                return "exception";
        }

        return "unknown";
    }

} // end of namespace
