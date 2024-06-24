#pragma once

#include "ospjolt/joltinteg.h"
#include <godot_cpp/classes/class_db_singleton.hpp>
#include <testapp/scenarios.h>
#include <godot_cpp/classes/node3d.hpp>
#include <testapp/testapp.h>

namespace godot {
	using namespace testapp;

class FlyingScene : public Node3D {
	GDCLASS(FlyingScene, Node3D)

private:
	//using ExecutorType = SingleThreadedExecutor;

	RID m_scenario;
	RID m_viewport;
	RID m_mainCamera;
	RID m_lightInstance;
	ospjolt::ACtxJoltWorld* m_joltWorld;

	//TestApp         m_testApp;
    //MainLoopControl m_MainLoopCtrl;

    //MainLoopSignals m_signals;

	//void load_a_bunch_of_stuff();
	
protected:
	static void _bind_methods();

public:
	FlyingScene();
	~FlyingScene();

	//void _process(double delta) override;
	void _enter_tree()          			override;
	void _ready()               			override;
	void _physics_process(double delta)     override;
};

}