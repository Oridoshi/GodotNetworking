add_library(customlib_reseaux_client SHARED
        gn_network_manager.cpp
        register_types.cpp
)

target_include_directories(customlib_reseaux_client PUBLIC include)

target_link_libraries(customlib_reseaux_client PRIVATE godot-cpp)