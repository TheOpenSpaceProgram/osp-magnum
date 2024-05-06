/**
 * Open Space Program
 * Copyright © 2019-2021 Open Space Program Project
 *
 * MIT License
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#pragma once

#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_INFO
#include <spdlog/spdlog.h>

namespace osp
{
using Logger_t = std::shared_ptr<spdlog::logger>;

inline thread_local Logger_t t_logger; // Unique logger per thread

inline void set_thread_logger(Logger_t logger)
{
    t_logger = std::move(logger);
}

} // namespace osp

#define OSP_LOG_TRACE(...) SPDLOG_LOGGER_TRACE(osp::t_logger, __VA_ARGS__)
#define OSP_LOG_DEBUG(...) SPDLOG_LOGGER_DEBUG(osp::t_logger, __VA_ARGS__)
#define OSP_LOG_INFO(...) SPDLOG_LOGGER_INFO(osp::t_logger, __VA_ARGS__)
#define OSP_LOG_WARN(...) SPDLOG_LOGGER_TRACE(osp::t_logger, __VA_ARGS__)
#define OSP_LOG_ERROR(...) SPDLOG_LOGGER_ERROR(osp::t_logger, __VA_ARGS__)
#define OSP_LOG_CRITICAL(...) SPDLOG_LOGGER_CRITICAL(osp::t_logger, __VA_ARGS__)
