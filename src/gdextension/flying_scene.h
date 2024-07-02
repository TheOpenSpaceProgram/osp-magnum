#pragma once

#include "osp/util/UserInputHandler.h"

#include <godot_cpp/classes/class_db_singleton.hpp>
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/thread.hpp>
#include <sstream>
#include <testapp/scenarios.h>
#include <testapp/testapp.h>

namespace godot
{
using namespace testapp;
using namespace osp::input;
class FlyingScene : public Node3D
{
    GDCLASS(FlyingScene, Node3D)

private:
    using ExecutorType = SingleThreadedExecutor;

    RID               m_scenario;
    RID               m_viewport;
    RID               m_lightInstance;

    TestApp           m_testApp;
    MainLoopControl  *m_mainLoopCtrl;

    MainLoopSignals   m_signals;

    std::stringstream m_dbgStream;
    std::stringstream m_errStream;
    std::stringstream m_warnStream;

    String            m_scene;

    ExecutorType      m_executor;
    UserInputHandler *m_pUserInput;

    void              load_a_bunch_of_stuff();
    void              setup_app();
    void              draw_event();
    void              destroy_app();

    void              signal_all()
    {
        m_testApp.m_pExecutor->signal(m_testApp, m_signals.mainLoop);
        m_testApp.m_pExecutor->signal(m_testApp, m_signals.inputs);
        m_testApp.m_pExecutor->signal(m_testApp, m_signals.renderSync);
        m_testApp.m_pExecutor->signal(m_testApp, m_signals.renderResync);
        m_testApp.m_pExecutor->signal(m_testApp, m_signals.sceneUpdate);
        m_testApp.m_pExecutor->signal(m_testApp, m_signals.sceneRender);
    }

protected:
    static void _bind_methods();

public:
    FlyingScene();
    ~FlyingScene();

    // void _process(double delta) override;
    void              _enter_tree() override;
    void              _exit_tree() override;
    void              _ready() override;
    void              _physics_process(double delta) override;
    void              _process(double delta) override;
    void              _input(const Ref<InputEvent> &input) override;

    void              set_scene(String const &scene);
    String const     &get_scene() const;

    inline godot::RID get_main_scenario()
    {
        return m_scenario;
    };
    inline godot::RID get_main_viewport()
    {
        return m_viewport;
    };

    inline void set_user_input(UserInputHandler *pUserInput)
    {
        m_pUserInput = pUserInput;
    }
    inline void set_ctrl(MainLoopControl *mainLoopCtrl, MainLoopSignals const &signals)
    {
        m_mainLoopCtrl = mainLoopCtrl;
        m_signals      = signals;
    }
};

} // namespace godot