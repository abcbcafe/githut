#pragma once

// Thin logging helpers that route through Godot's logging so messages show up in
// the editor Output panel and the console. Keep this dependency-light: it is used
// from every subsystem.

#include <godot_cpp/variant/utility_functions.hpp>

#define PT_LOG(...)  ::godot::UtilityFunctions::print("[pathtracer] ", __VA_ARGS__)
#define PT_WARN(...) ::godot::UtilityFunctions::push_warning("[pathtracer] ", __VA_ARGS__)
#define PT_ERR(...)  ::godot::UtilityFunctions::push_error("[pathtracer] ", __VA_ARGS__)
