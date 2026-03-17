add_library(GodoNet_client SHARED
        gn_network_manager.cpp
        register_types.cpp
        ../commun/protocol.hpp
)

target_link_libraries(GodoNet_client PRIVATE
        godot::cpp
        $<$<PLATFORM_ID:Windows>:ws2_32>
)

target_include_directories(GodoNet_client PUBLIC include)

# --- CONFIGURATION DES CHEMINS ---

# Dossier racine du projet Godot
set(GODOT_PROJECT_DIR "${CMAKE_SOURCE_DIR}/game")

# Dossier où sera placé le fichier .gdextension
set(GEXT_OUTPUT_DIR "${GODOT_PROJECT_DIR}/bin")

# Dossier spécifique à l'OS pour la DLL (Style Prof)
if(APPLE)
    set(DLL_OUTPUT_DIR "${GEXT_OUTPUT_DIR}/macos/arm64")
elseif(WIN32)
    set(DLL_OUTPUT_DIR "${GEXT_OUTPUT_DIR}/windows")
else()
    set(DLL_OUTPUT_DIR "${GEXT_OUTPUT_DIR}/linux")
endif()

set(GODOT_EXTENSION_NAME "network.gdextension")

# --- COMMANDE DE COPIE ---
add_custom_command(TARGET GodoNet_client POST_BUILD
        # 1. Création des dossiers nécessaires
        COMMAND ${CMAKE_COMMAND} -E make_directory "${DLL_OUTPUT_DIR}"

        # 2. Copie de la DLL dans bin/windows/
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:GodoNet_client>
        "${DLL_OUTPUT_DIR}/$<TARGET_FILE_NAME:GodoNet_client>"

        # 3. Copie du template vers bin/network.gdextension (Un dossier au-dessus)
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/godotnet.gdextension.in"
        "${GEXT_OUTPUT_DIR}/${GODOT_EXTENSION_NAME}"

        COMMENT "Installation de la GDExtension : Config dans bin/ et DLL dans ${DLL_OUTPUT_DIR}"
)