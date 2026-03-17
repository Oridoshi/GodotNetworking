add_executable(GodoNet_server
        server_core.cpp
        include/server_core.hpp
        ../commun/protocol.hpp
)

target_include_directories(GodoNet_server PUBLIC include)

target_link_libraries(GodoNet_server PRIVATE
        EnTT::EnTT
        $<$<PLATFORM_ID:Windows>:ws2_32>
)