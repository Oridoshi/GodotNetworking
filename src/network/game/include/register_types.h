#pragma once

#include <godot_cpp/core/class_db.hpp>

using namespace godot;

void initialize_network_module(ModuleInitializationLevel p_level);
void uninitialize_network_module(ModuleInitializationLevel p_level);