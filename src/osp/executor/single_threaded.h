#pragma once

#include <osp/framework/framework.h>

#include <spdlog/logger.h>

#include <memory>

namespace osp::exec
{

class SingleThreadedExecutor final : public osp::fw::IExecutor
{
public:

    void load(osp::fw::Framework& rFW) override
    {

    }

    void run(osp::fw::Framework& rFW, osp::PipelineId pipeline) override
    {

    }

    void signal(osp::fw::Framework& rFW, osp::PipelineId pipeline) override
    {

    }

    void wait(osp::fw::Framework& rFW) override
    {

    }

    bool is_running(osp::fw::Framework const& rFW) override
    {
        return false;
    }

    std::shared_ptr<spdlog::logger> m_log;

private:

};

}
