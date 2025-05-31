/**
 * Open Space Program
 * Copyright © 2019-2024 Open Space Program Project
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
#include "builder.h"

#include "../util/logging.h"

#include <sstream>

namespace osp::fw
{

ContextBuilder::~ContextBuilder()
{
    LGRN_ASSERTM(m_finalized, "ContextBuilder::finalize must be used before destruction");
}

void ContextBuilder::add_feature(FeatureDef const &def, entt::any setupData) noexcept
{
    LGRN_ASSERT( ! m_finalized );

    FSessionId const fsessionId = m_rFW.m_fsessionIds.create();

    m_rFW.resize_ctx();
    FeatureContext &rCtx = m_rFW.m_contextData[m_ctx];
    rCtx.sessions.push_back(fsessionId);

    m_rFW.m_fsessionData.resize(m_rFW.m_fsessionIds.capacity());
    FeatureSession &rFSession = m_rFW.m_fsessionData[fsessionId];


    for (FeatureDef::FIRelationship const& relation : def.relationships)
    {
        FITypeInfo const& subjectInfo = m_rInfo.info_for(relation.subject);

        if (relation.type == FeatureDef::ERelationType::DependOn)
        {
            FIInstanceId const found = find_dependency(relation.subject);

            if (found.has_value())
            {
                rFSession.finterDependsOn.push_back(found);
            }
            else
            {
                // Not found, ERROR!
                m_errors.emplace_back(ErrDependencyNotFound{
                    .whileAdding = def.name,
                    .requiredFI  = subjectInfo.name
                });
            }
        }
        else if (relation.type == FeatureDef::ERelationType::Implement)
        {
            FIInstanceId &rFInterId = rCtx.finterSlots[relation.subject];

            if (rFInterId.has_value())
            {
                // Already exists, error
                m_errors.emplace_back(ErrAlreadyImplemented{
                    .whileAdding   = def.name,
                    .alreadyImplFI = subjectInfo.name
                });
            }
            else
            {
                rFInterId = m_rFW.m_fiinstIds.create();
                rFSession.finterImplements.push_back(rFInterId);
                m_rFW.m_fiinstData.resize(m_rFW.m_fiinstIds.capacity());
                FeatureInterface &rFI = m_rFW.m_fiinstData[rFInterId];

                rFI.context = m_ctx;
                rFI.type    = relation.subject;

                // Make LoopBlockIds
                rFI.loopblks.resize(subjectInfo.loopblkCount);
                m_rFW.m_tasks.loopblkIds.create(rFI.loopblks.begin(), rFI.loopblks.end());
                m_rFW.m_tasks.loopblkInst.resize(m_rFW.m_tasks.loopblkIds.capacity());

                // Make DataIds
                rFI.data.resize(subjectInfo.dataCount);
                m_rFW.m_dataIds.create(rFI.data.begin(), rFI.data.end());
                m_rFW.m_data.resize(m_rFW.m_dataIds.capacity());

                // Make PipelineIds
                rFI.pipelines.resize(subjectInfo.pipelines.size());
                m_rFW.m_tasks.pipelineIds.create(rFI.pipelines.begin(), rFI.pipelines.end());
                m_rFW.m_tasks.pipelineInst.resize(m_rFW.m_tasks.pipelineIds.capacity());
                auto const &pltypeReg = PipelineTypeIdReg::instance();
                for (std::size_t i = 0; i < subjectInfo.pipelines.size(); ++i)
                {
                    PipelineId const plId    = rFI.pipelines[i];
                    auto       const defInfo = subjectInfo.pipelines[i];
                    m_rFW.m_tasks.pipelineInst[plId] = {
                        .name              = defInfo.name,
                        .type              = defInfo.type,
                        .block             = {}, // assigned in feature def function body.
                        .scheduleCondition = {}, // same here
                        .initialStage      = pltypeReg.get(defInfo.type).initialStage
                    };
                }

                // Make TaskIds
                rFI.tasks.resize(subjectInfo.taskCount);
                m_rFW.m_tasks.taskIds.create(rFI.tasks.begin(), rFI.tasks.end());
                m_rFW.m_taskImpl        .resize(m_rFW.m_tasks.taskIds.capacity());
                m_rFW.m_tasks.taskInst  .resize(m_rFW.m_tasks.taskIds.capacity());
                rFSession.tasks.insert(rFSession.tasks.end(), rFI.tasks.begin(), rFI.tasks.end());
            }
        }
    }

    if ( ! m_errors.empty() )
    {
        return; // in error state, do nothing
    }

    FeatureBuilder fb{
        .m_rFW      = m_rFW,
        .rSession   = rFSession,
        .sessionId  = fsessionId,
        .ctx        = m_ctx,
        .ctxScope   = arrayView<ContextId const>(m_ctxScope) };

    def.func(fb, std::move(setupData));

    for (FeatureDef::FIRelationship const& relation : def.relationships)
    {
        if (relation.type == FeatureDef::ERelationType::Implement)
        {
            FeatureInterface const &rFI = m_rFW.m_fiinstData[rCtx.finterSlots[relation.subject]];
            for (PipelineId const pipeline : rFI.pipelines)
            {
                if ( ! m_rFW.m_tasks.pipelineInst[pipeline].block.has_value() )
                {
                    m_errors.emplace_back(ErrPipelineWithNoParentLoopBlock{.whileAdding = def.name, .pipeline = pipeline});
                }
            }
        }
    }
}


bool ContextBuilder::finalize(ContextBuilder &&eat)
{
    eat.m_finalized = true;

    if (eat.has_error())
    {
        // Log errors to the terminal
        std::ostringstream os;

        auto const visitErr = [&os, &eat] (auto&& err)
        {
            using ERR_T = std::decay_t<decltype(err)>;
            if constexpr (std::is_same_v<ERR_T, ContextBuilder::ErrAlreadyImplemented>)
            {
                os << "* \"" << err.whileAdding << "\": " << "Feature Interface \""<< err.alreadyImplFI << "\" is already implemented\n";
            }
            else if constexpr (std::is_same_v<ERR_T, ContextBuilder::ErrDependencyNotFound>)
            {
                os << "* \"" << err.whileAdding << "\": " << "Feature Interface dependency \""<< err.requiredFI << "\" is not found\n";
            }
            else if constexpr (std::is_same_v<ERR_T, ContextBuilder::ErrPipelineWithNoParentLoopBlock>)
            {
                os << "* \"" << err.whileAdding << "\": " << "No Parent LoopBlock assigned to pipeline \""
                   << eat.m_rFW.m_tasks.pipelineInst[err.pipeline].name
                   << "\" (" << err.pipeline.value << ")\n";
            }
        };


        for (ContextBuilder::Error_t const& err : eat.m_errors)
        {
            std::visit(visitErr, err);
        }

        OSP_LOG_ERROR("Errors while adding features:\n{}", os.str());

        // TODO: Undo changes by keeping track of and removing all the features added by this
        //       context builder. just abort for now
        std::abort();

        return false;
    }
    return true;
}

} // namespace osp::fw
