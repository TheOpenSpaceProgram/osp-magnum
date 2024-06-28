#pragma once

#include <godot_cpp/core/class_db.hpp>

namespace godot
{

void initialize_example_module(ModuleInitializationLevel p_level);
void uninitialize_example_module(ModuleInitializationLevel p_level);

} // namespace godot