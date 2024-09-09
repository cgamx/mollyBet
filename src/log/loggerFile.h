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
#include <memory>

//-------------------------------------
namespace MindShake {

    // Simple file logger
    //---------------------------------
    class LoggerFile : LoggerInterface {
        using unique_file = std::unique_ptr<std::FILE, decltype(&fclose)>;

        public:
            explicit LoggerFile(const char *filename, bool append=false) : mFile(fopen(filename, append ? "a" : "w"), &fclose) {
                if (mFile == nullptr) {
                    Logger::log(LogLevel::Error, "Cannot create file: '{}'", filename);
                }
            }

            void log(LogLevel level, const char *msg) const override {
                if (mFile) {
                    fprintf(mFile.get(), "[%9s] %s\n", Logger::getLevelName(level), msg);
                    fflush(mFile.get());
                }
            }

        protected:
            unique_file mFile;
    };

} // end of namespace