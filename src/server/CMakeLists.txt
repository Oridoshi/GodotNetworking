add_executable(GodoNet_server
        server_core.cpp
        include/server_core.hpp
        include/protocol.hpp
)

target_include_directories(GodoNet_server PUBLIC include)

target_link_libraries(GodoNet_server PRIVATE
        EnTT::EnTT
        $<$<PLATFORM_ID:Windows>:ws2_32>
)


# Synchronisation du fichier protocol.hpp entre le serveur et le client
# 1. Définition des chemins
set(PROTOCOL_SRC "${CMAKE_CURRENT_SOURCE_DIR}/include/protocol.hpp")
set(PROTOCOL_DST "${CMAKE_CURRENT_SOURCE_DIR}/../game/include/protocol.hpp")

message(STATUS "DEBUG: Protocol Source: ${PROTOCOL_SRC}")
message(STATUS "DEBUG: Protocol Destination: ${PROTOCOL_DST}")

# 2. Création de la commande de copie
# Elle s'exécutera APRÈS la compilation du serveur
add_custom_command(TARGET GodoNet_server POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${PROTOCOL_SRC}"
        "${PROTOCOL_DST}"
        COMMENT "Syncing protocol.hpp to game client..."
)

