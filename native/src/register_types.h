#pragma once

#include <godot_cpp/core/class_db.hpp>

// GDExtension entry points. Declared here, implemented in register_types.cpp and
// referenced by the GDExtension descriptor (game/addons/pathtracer/pathtracer.gdextension).

void initialize_pathtracer_module(godot::ModuleInitializationLevel p_level);
void uninitialize_pathtracer_module(godot::ModuleInitializationLevel p_level);
