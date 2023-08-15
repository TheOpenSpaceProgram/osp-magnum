#include <osp/tasks/top_worker.h>

namespace testapp
{





} // namespace testapp
/**
 * @brief Close sessions, delete all their associated TopData, Tasks, and Targets.
 */
void top_close_session(Tasks& rTasks, TaskGraph const& graph, TopTaskDataVec_t& rTaskData, ArrayView<entt::any> topData, ExecContext& rExec, ArrayView<Session> sessions);



void top_close_session(
        Tasks &                 rTasks,
        TaskGraph const&        graph,
        TopTaskDataVec_t&       rTaskData,
        ArrayView<entt::any>    topData,
        ExecContext&            rExec,
        ArrayView<Session>      sessions)
{
    // Run cleanup pipelines
    for (Session &rSession : sessions)
    {
        if (rSession.m_cleanup.pipeline != lgrn::id_null<PipelineId>())
        {
            exec_request_run(rExec, rSession.m_cleanup.pipeline);
        }
    }
    exec_update(rTasks, graph, rExec);
    top_run_blocking(rTasks, graph, rTaskData, topData, rExec);

    // Clear each session's TopData
    for (Session &rSession : sessions)
    {
        for (TopDataId const id : std::exchange(rSession.m_data, {}))
        {
            if (id != lgrn::id_null<TopDataId>())
            {
                topData[std::size_t(id)].reset();
            }
        }
    }

    // Clear each session's tasks
    for (Session &rSession : sessions)
    {
        for (TaskId const task : rSession.m_tasks)
        {
            rTasks.m_taskIds.remove(task);

            TopTask &rCurrTaskData = rTaskData[task];
            rCurrTaskData.m_debugName.clear();
            rCurrTaskData.m_dataUsed.clear();
            rCurrTaskData.m_func = nullptr;
        }
    }
}
