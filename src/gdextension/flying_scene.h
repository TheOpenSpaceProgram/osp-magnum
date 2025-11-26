#pragma once

#include <godot_cpp/classes/class_db_singleton.hpp>
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/thread.hpp>
#include <sstream>

#include <adera_app/application.h>
#include <osp/core/resourcetypes.h>
#include <osp/framework/executor.h>
#include <osp/framework/framework.h>
#include <osp/util/UserInputHandler.h>

namespace godot
{
using namespace osp::input;
class FlyingScene : public Node3D
{
    GDCLASS(FlyingScene, Node3D)

private:
    using ExecutorType = osp::fw::SingleThreadedExecutor;

    struct UpdateParams {
        float deltaTimeIn;
        bool update;
        bool sceneUpdate;
        bool resync;
        bool sync;
        bool render;
    };

    RID               m_scenario;
    RID               m_viewport;
    RID               m_lightInstance;
    RID               m_light;

    //TestApp           m_testApp;
    //adera::MainLoopControl  *m_mainLoopCtrl;

    //MainLoopSignals   m_signals;

    std::stringstream m_dbgStream;
    std::stringstream m_errStream;
    std::stringstream m_warnStream;

    String            m_scene;

    ExecutorType      m_executor;
    //UserInputHandler  m_userInput;

    osp::fw::Framework m_framework;
    osp::fw::ContextId m_mainContext;
    //osp::fw::IExecutor *m_pExecutor  { nullptr };
    osp::PkgId         m_defaultPkg  { lgrn::id_null<osp::PkgId>() };

    void              clear_resource_owners();

    // testapp has drive_default_main_loop(), but this is mostly for driving the CLI, but there's
    // no equivalent here

    void drive_scene_cycle(UpdateParams p);
    void run_context_cleanup(osp::fw::ContextId);

    void              load_a_bunch_of_stuff();
    void              setup_app();
    void              draw_event();
    void              destroy_app();

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

//    inline void set_user_input(UserInputHandler *pUserInput)
//    {
//        m_pUserInput = pUserInput;
//    }
//    inline void set_ctrl(adera::MainLoopControl *mainLoopCtrl)
//    {
//        m_mainLoopCtrl = mainLoopCtrl;
//        //m_signals      = signals;
//    }
};

} // namespace godot
