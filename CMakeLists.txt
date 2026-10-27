cmake_minimum_required(VERSION 3.25.0)
cmake_policy(SET CMP0072 NEW)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Must be set before project() so vcpkg can install the optional feature.
option(ONLINE_MODE_AUTHENTICATION "Allows authentication with Mojang servers (requires libcurl)" ON)
option(BETACRAFT_HEARTBEAT "Ping the Betacraft server list (requires libcurl and nlohmann_json)" OFF)
option(CRASH_LOGGING "Lets the server catch crashes and create crash reports" ON)
option(EXPERIMENTAL "Experimental or work-in-progress features/changes" OFF)
set(GENERATION_PRECISION "DOUBLE" CACHE STRING "Reduces perlin and simplex noise precision (all values other than DOUBLE are experimental and not really supported)")
set_property(CACHE GENERATION_PRECISION PROPERTY STRINGS "DOUBLE" "FLOAT" "LONG" "INT")

option(BUILD_SERVER "Build as server" ON)
option(SDL_VENDORED "Use vendored (locally built) SDL library" ON)

# Nintendo 3DS / Switch homebrew (devkitPro). Detected from the toolchain file
# (see BUILDING_CONSOLE.md); BPP_TARGET=3DS|SWITCH forces it for odd setups.
set(BPP_TARGET "" CACHE STRING "Force a console target: 3DS or SWITCH (normally auto-detected)")
set(BPP_TARGET_3DS OFF)
set(BPP_TARGET_SWITCH OFF)
if(BPP_TARGET STREQUAL "3DS" OR CMAKE_TOOLCHAIN_FILE MATCHES "[/\\\\]3DS\\.cmake$")
    set(BPP_TARGET_3DS ON)
elseif(BPP_TARGET STREQUAL "SWITCH" OR CMAKE_TOOLCHAIN_FILE MATCHES "[/\\\\]Switch\\.cmake$")
    set(BPP_TARGET_SWITCH ON)
endif()
if(BPP_TARGET_3DS OR BPP_TARGET_SWITCH)
    set(BPP_TARGET_CONSOLE ON)
    message(STATUS "Building for a homebrew console target: server only, optional features disabled")
    # No client (SDL3/OpenGL), no libcurl/vcpkg, and CrashCatch needs fork()/execinfo
    set(BUILD_SERVER ON CACHE BOOL "Build as server" FORCE)
    set(ONLINE_MODE_AUTHENTICATION OFF CACHE BOOL "" FORCE)
    set(BETACRAFT_HEARTBEAT OFF CACHE BOOL "" FORCE)
    set(CRASH_LOGGING OFF CACHE BOOL "" FORCE)
else()
    set(BPP_TARGET_CONSOLE OFF)
endif()

if(BETACRAFT_HEARTBEAT)
    list(APPEND VCPKG_MANIFEST_FEATURES "betacraft-heartbeat")
endif()
# curl/glm/sdl3 are only needed for a subset of configurations; pulling them
# in unconditionally used to make vcpkg build (and the client link against)
# libraries that particular builds never use.
if(ONLINE_MODE_AUTHENTICATION OR BETACRAFT_HEARTBEAT)
    list(APPEND VCPKG_MANIFEST_FEATURES "curl-support")
endif()
if(NOT BUILD_SERVER)
    list(APPEND VCPKG_MANIFEST_FEATURES "client")
    if(NOT SDL_VENDORED)
        list(APPEND VCPKG_MANIFEST_FEATURES "client-sdl3")
    endif()
endif()

project(Betrock++ VERSION 0.2.13)

# Drop fortify for Alpine
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND EXISTS "/usr/include/fortify/stdio.h")
    add_compile_options(-U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0)
endif()

if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
    message(STATUS "No build type specified, defaulting to RelWithDebInfo")

    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
        "Debug"
        "Release"
        "RelWithDebInfo"
        "MinSizeRel")
endif()

find_package(libdeflate CONFIG REQUIRED)

# Building as a client, we need this extra stuff
# (BUILD_SERVER / SDL_VENDORED are declared above project(), so vcpkg only
# fetches what this particular configuration actually needs.)

if(BUILD_SERVER)
message(STATUS "Building as Server")
else()
message(STATUS "Building as Client")

if(SDL_VENDORED)
include(FetchContent)

FetchContent_Declare(
    SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG        release-3.4.12
    GIT_SHALLOW    TRUE
)

FetchContent_MakeAvailable(SDL3)
else()
find_package(SDL3 CONFIG REQUIRED)
endif()

find_package(OpenGL REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(Threads REQUIRED)
endif()

# Try to find Git, but don't fail if it's missing
find_package(Git QUIET)

# TODO: 
if(GIT_FOUND)
    # Get the latest Git commit hash
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE PROJECT_GIT_COMMIT
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )

    # Get the current Git branch name
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse --abbrev-ref HEAD
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE PROJECT_GIT_BRANCH
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
else()
    message(WARNING "Git was not found. Version information will not include commit or branch data.")

    set(PROJECT_GIT_COMMIT "unknown")
    set(PROJECT_GIT_BRANCH "unknown")
endif()

# Configure the version header file
configure_file(
    ${CMAKE_SOURCE_DIR}/src/bpp_shared/version.h.in
    ${CMAKE_SOURCE_DIR}/src/bpp_shared/version.h
)

# Command-file wildcard
file(GLOB COMMAND_SOURCES
    "src/bpp_server/commands/implementation/command_*.cpp"
)

# Shared source-code
set(SHARED_SOURCES
    # Shared files

    # Tile entities
    src/bpp_shared/tile_entities/tile_entity_manager.cpp
    src/bpp_shared/tile_entities/tile_entity_base.cpp
    src/bpp_shared/tile_entities/tile_entity_sign.cpp
    src/bpp_shared/tile_entities/tile_entity_chest.cpp
    src/bpp_shared/tile_entities/tile_entity_dispenser.cpp
    src/bpp_shared/tile_entities/tile_entity_mob_spawner.cpp
    src/bpp_shared/tile_entities/tile_entity_furnace.cpp
    src/bpp_shared/tile_entities/tile_entity_jukebox.cpp
    src/bpp_shared/tile_entities/tile_entity_noteblock.cpp
    src/bpp_shared/tile_entities/tile_entity_piston.cpp

    # Entities
    src/bpp_shared/pathfinding/pathfinder.cpp
    src/bpp_shared/entities/entity_squid.cpp
    src/bpp_shared/entities/entity.cpp
    src/bpp_shared/entities/entity_item.cpp
    src/bpp_shared/entities/entity_player.cpp
    src/bpp_shared/entities/entity_mobile.cpp
    src/bpp_shared/entities/entity_manager.cpp
    src/bpp_shared/entities/entity_pig.cpp
    src/bpp_shared/entities/entity_cow.cpp
    src/bpp_shared/entities/entity_sheep.cpp
    src/bpp_shared/entities/entity_chicken.cpp
    src/bpp_shared/entities/entity_falling_block.cpp
    src/bpp_shared/entities/entity_animal.cpp
    src/bpp_shared/entities/entity_mob.cpp
    src/bpp_shared/entities/entity_zombie.cpp
    src/bpp_shared/entities/entity_hostile.cpp
    src/bpp_shared/entities/entity_skeleton.cpp
    src/bpp_shared/entities/entity_creeper.cpp
    src/bpp_shared/entities/entity_spider.cpp
    src/bpp_shared/entities/entity_boat.cpp
    src/bpp_shared/entities/entity_minecart.cpp
    src/bpp_shared/entities/entity_painting.cpp
    src/bpp_shared/entities/entity_arrow.cpp
    src/bpp_shared/entities/entity_fishing_bobber.cpp
    src/bpp_shared/entities/entity_throwable.cpp
    src/bpp_shared/entities/entity_slime.cpp
    src/bpp_shared/entities/entity_tnt.cpp
    src/bpp_shared/entities/entity_wolf.cpp
    src/bpp_shared/entities/entity_lightning_bolt.cpp

    # NBT
    src/bpp_shared/nbt/nbt_textifier.cpp

    # World
    src/bpp_shared/world/chunk.cpp
    src/bpp_shared/world/world_wrapper.cpp
    src/bpp_shared/world/world.cpp
    src/bpp_shared/world/spawner.cpp
    src/bpp_shared/world/lighter.cpp
    src/bpp_shared/world/storage/region.cpp
    src/bpp_shared/world/storage/region_manager.cpp
    src/bpp_shared/world/managers/weather_system.cpp
    src/bpp_shared/world/managers/tick_scheduler.cpp
    src/bpp_shared/world/managers/redstone_manager.cpp
    src/bpp_shared/world/managers/rail_manager.cpp

    # Blocks
    src/bpp_shared/blocks/block_properties_behaviors.cpp
    src/bpp_shared/blocks/block_behaviors.cpp
    src/bpp_shared/blocks/block_behaviors/common.cpp
    src/bpp_shared/blocks/block_behaviors/fluids.cpp
    src/bpp_shared/blocks/block_behaviors/rails.cpp
    src/bpp_shared/blocks/block_behaviors/redstone.cpp
    src/bpp_shared/blocks/block_behaviors/plants.cpp
    src/bpp_shared/blocks/block_behaviors/doors.cpp
    src/bpp_shared/blocks/block_behaviors/beds.cpp
    src/bpp_shared/blocks/block_behaviors/chests.cpp
    src/bpp_shared/blocks/block_behaviors/falling_blocks.cpp
    src/bpp_shared/blocks/block_behaviors/portal.cpp
    src/bpp_shared/blocks/block_behaviors/fire.cpp
    src/bpp_shared/blocks/block_behaviors/misc.cpp
    src/bpp_shared/blocks/block_behaviors/drops.cpp
    src/bpp_shared/blocks/block_behaviors/piston.cpp
    src/bpp_shared/blocks/block_properties.cpp

    # Items
    src/bpp_shared/items/item_map.cpp
    src/bpp_shared/items/item_properties.cpp
    src/bpp_shared/items/tool_properties.cpp
    src/bpp_shared/items/tool_item_properties.cpp

    # Inventory interactions
    src/bpp_shared/inventory/inventory_interaction.cpp
    src/bpp_shared/inventory/interactions/chest.cpp
    src/bpp_shared/inventory/interactions/large_chest.cpp
    src/bpp_shared/inventory/interactions/crafting.cpp
    src/bpp_shared/inventory/interactions/furnace.cpp
    src/bpp_shared/inventory/interactions/player.cpp
    src/bpp_shared/inventory/interactions/crafting_table.cpp
    src/bpp_shared/inventory/interactions/dispenser.cpp

    # Crafting
    src/bpp_shared/crafting/recipe_manager.cpp
    src/bpp_shared/crafting/vanilla_recipes.cpp

    # Msc
    src/bpp_shared/teleporter.cpp
    src/bpp_shared/direction.cpp
    src/bpp_shared/gamerules.cpp

    # Generation
    src/bpp_shared/world/noise/noise_generator.cpp
    src/bpp_shared/world/noise/noise_perlin.cpp
    src/bpp_shared/world/noise/noise_simplex.cpp
    src/bpp_shared/world/generator/generator.cpp

    # Overworld generator
    src/bpp_shared/world/generator/overworld/chunk_gen.cpp
    src/bpp_shared/world/generator/overworld/tree_gen.cpp
    src/bpp_shared/world/generator/overworld/biome_gen.cpp
    src/bpp_shared/world/generator/shared/cave_gen.cpp
    src/bpp_shared/world/generator/shared/features/feature_clay.cpp
    src/bpp_shared/world/generator/shared/features/feature_dungeon.cpp
    src/bpp_shared/world/generator/shared/features/feature_fire.cpp
    src/bpp_shared/world/generator/shared/features/feature_glowstone.cpp
    src/bpp_shared/world/generator/shared/features/feature_lake.cpp
    src/bpp_shared/world/generator/shared/features/feature_liquid.cpp
    src/bpp_shared/world/generator/shared/features/feature_ore.cpp
    src/bpp_shared/world/generator/shared/features/feature_plants.cpp

    # Nether generator
    src/bpp_shared/world/generator/nether/chunk_gen.cpp

    # Logger
    src/bpp_shared/logger/logger.cpp
    src/bpp_shared/logger/logstream.cpp
    src/bpp_shared/logger/style.cpp
    src/bpp_shared/config/config.cpp
    src/bpp_shared/config/list_parser.cpp

    # Utilities
    src/bpp_utilities/utilities.cpp
    src/bpp_shared/helpers/explosion.cpp
    src/bpp_shared/helpers/direction_fixer.cpp
    src/bpp_shared/enums/biomes.cpp
    src/bpp_shared/strings/ucs2.cpp
    src/bpp_shared/strings/labels.cpp
    src/bpp_shared/networking/network_stream.cpp
)

# Server-specific source code
set(SERVER_SOURCES
    src/bpp_server/server.cpp
    src/bpp_server/console/server_console.cpp
    src/bpp_server/internet/online_auth.cpp
    src/bpp_server/internet/betacraft_heartbeat.cpp
    src/bpp_server/packet/handle_packet.cpp
    src/bpp_server/player_conn/server_pconnstate_manager.cpp
    src/bpp_server/packet/packet_dispatcher.cpp
    src/bpp_server/chunk_io/chunk_broadcaster.cpp
    src/bpp_server/trackers/entity_tracker.cpp
    src/bpp_server/trackers/inventory_tracker.cpp
    src/bpp_server/entities/entity_mp_player.cpp
    src/bpp_server/blocks/server_block_behaviors.cpp
    src/bpp_server/entities/entity_dummy_player.cpp
    src/bpp_server/world/world_event_broadcaster.cpp

    src/bpp_server/commands/command.cpp
    src/bpp_server/commands/command_manager.cpp

    src/bpp_server/addon/addon_manager.cpp
    src/bpp_server/addon/addon_impl.cpp

    ${COMMAND_SOURCES}
)

# No terminal / line editor on the consoles, use the headless stand-in
if(BPP_TARGET_CONSOLE)
    list(REMOVE_ITEM SERVER_SOURCES src/bpp_server/console/server_console.cpp)
    list(APPEND SERVER_SOURCES src/bpp_server/console/server_console_headless.cpp)
endif()

# Client-specific source code
set(CLIENT_SOURCES
    src/bpp_client/client.cpp
    src/bpp_client/packet/handle_client_packet.cpp
    src/bpp_client/packet/client_packet_dispatcher.cpp

    src/bpp_client/single_header_impl/sokol_gfx.cpp
)

# Platform-specific source code
if(BPP_TARGET_3DS)
    file(GLOB PLATFORM_SOURCES CONFIGURE_DEPENDS
        "${CMAKE_SOURCE_DIR}/src/bpp_shared/platform/3ds/*.cpp"
    )
elseif(BPP_TARGET_SWITCH)
    file(GLOB PLATFORM_SOURCES CONFIGURE_DEPENDS
        "${CMAKE_SOURCE_DIR}/src/bpp_shared/platform/nx/*.cpp"
    )
elseif(WIN32)
    file(GLOB PLATFORM_SOURCES CONFIGURE_DEPENDS
        "${CMAKE_SOURCE_DIR}/src/bpp_shared/platform/win32/*.cpp"
    )
elseif(APPLE)
    file(GLOB PLATFORM_SOURCES CONFIGURE_DEPENDS
        "${CMAKE_SOURCE_DIR}/src/bpp_shared/platform/apple/*.cpp"
    )
elseif(CMAKE_SYSTEM_NAME STREQUAL "PS2")
    file(GLOB PLATFORM_SOURCES CONFIGURE_DEPENDS
        "${CMAKE_SOURCE_DIR}/src/bpp_shared/platform/ps2/*.cpp"
    )
else(UNIX)
    file(GLOB PLATFORM_SOURCES CONFIGURE_DEPENDS
        "${CMAKE_SOURCE_DIR}/src/bpp_shared/platform/unix/*.cpp"
    )
endif()

# Shared code
add_executable(BetrockPlusPlus
    src/main.cpp
    ${SHARED_SOURCES}
    ${PLATFORM_SOURCES}
)

# macOS strips debugger/profiler attach entitlements on every rebuild
if(APPLE)
    add_custom_command(TARGET BetrockPlusPlus POST_BUILD
        COMMAND codesign -s - -f --entitlements ${CMAKE_SOURCE_DIR}/debug.entitlements $<TARGET_FILE:BetrockPlusPlus>
        COMMENT "Re-signing with debug entitlements for Instruments/LLDB attach"
    )
endif()

# Includes
include_directories(
    "${PROJECT_SOURCE_DIR}/include"
    "${PROJECT_SOURCE_DIR}/src/bpp_server"
    "${PROJECT_SOURCE_DIR}/src/bpp_shared"
    "${PROJECT_SOURCE_DIR}/src/bpp_utilities"
    "${PROJECT_SOURCE_DIR}/src/bpp_shared/enums"
    "${PROJECT_SOURCE_DIR}/src/bpp_shared/enums/network"
    "${PROJECT_SOURCE_DIR}/src/bpp_shared/strings"
    "${PROJECT_SOURCE_DIR}/src/bpp_shared/logger"
    "${PROJECT_SOURCE_DIR}/src/bpp_shared/world"
    "${PROJECT_SOURCE_DIR}/src/bpp_shared/world/noise"
    "${PROJECT_SOURCE_DIR}/src/bpp_shared/helpers/java"
    "${PROJECT_SOURCE_DIR}/src/bpp_shared/helpers"
)

# Only do -rdynamic for specific conditions
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_BUILD_TYPE MATCHES "^(Debug|RelWithDebInfo)$")
    target_link_options(BetrockPlusPlus PRIVATE -rdynamic)
endif()

if(BUILD_SERVER)
if(NOT BPP_TARGET_CONSOLE)
find_package(replxx CONFIG QUIET)
if(NOT replxx_FOUND)
    include(FetchContent)
    set(REPLXX_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(REPLXX_BUILD_PACKAGE OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        replxx
        GIT_REPOSITORY https://github.com/AmokHuginnsson/replxx.git
        GIT_TAG        release-0.0.4
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(replxx)
endif()
target_link_libraries(BetrockPlusPlus PRIVATE replxx::replxx)
endif()
# Server mode
    target_compile_definitions(BetrockPlusPlus PRIVATE
        BUILD_SERVER
        NOMINMAX
        WIN32_LEAN_AND_MEAN
    )
    target_sources(BetrockPlusPlus PRIVATE
        ${SERVER_SOURCES}
    )

    if(ONLINE_MODE_AUTHENTICATION OR BETACRAFT_HEARTBEAT)
        find_package(CURL REQUIRED)
        target_link_libraries(BetrockPlusPlus PRIVATE CURL::libcurl)
        target_sources(BetrockPlusPlus PRIVATE
            src/bpp_server/internet/curl_runtime.cpp
        )
    endif()

    if(ONLINE_MODE_AUTHENTICATION)
        target_compile_definitions(BetrockPlusPlus PRIVATE ONLINE_MODE_AUTHENTICATION)
    endif()

    if(BETACRAFT_HEARTBEAT)
        find_package(nlohmann_json CONFIG QUIET)
        if(NOT nlohmann_json_FOUND)
            message(STATUS "nlohmann_json package not found; fetching nlohmann/json via FetchContent")
            include(FetchContent)
            FetchContent_Declare(
                nlohmann_json
                URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
            )
            set(JSON_BuildTests OFF CACHE INTERNAL "")
            set(JSON_Install OFF CACHE INTERNAL "")
            FetchContent_MakeAvailable(nlohmann_json)
        endif()
        target_compile_definitions(BetrockPlusPlus PRIVATE BETACRAFT_HEARTBEAT)
        target_link_libraries(BetrockPlusPlus PRIVATE nlohmann_json::nlohmann_json)
    endif()
else()
    if (BETACRAFT_HEARTBEAT)
        message(WARNING "BETACRAFT_HEARTBEAT has no effect when BUILD_SERVER is OFF; ignoring it")
    endif()
#Client mode
    target_compile_definitions(BetrockPlusPlus PRIVATE
        BUILD_CLIENT
        NOMINMAX
        WIN32_LEAN_AND_MEAN
        GLFW_INCLUDE_NONE
    )
    target_sources(BetrockPlusPlus PRIVATE
        ${CLIENT_SOURCES}
    )
    target_link_libraries(BetrockPlusPlus PRIVATE
        SDL3::SDL3
        glm::glm
        OpenGL::GL
        Threads::Threads
    )
endif()

# Precompiled headers
if(BUILD_SERVER)
    target_precompile_headers(BetrockPlusPlus PRIVATE
        <format>
        "${PROJECT_SOURCE_DIR}/src/bpp_shared/nbt/nbt.h"
        "${PROJECT_SOURCE_DIR}/src/bpp_server/server.h"
    )
else()
    target_precompile_headers(BetrockPlusPlus PRIVATE
        <format>
        "${PROJECT_SOURCE_DIR}/src/bpp_shared/nbt/nbt.h"
    )
endif()

# Shared libraries
target_link_libraries(BetrockPlusPlus
    PRIVATE
    $<IF:$<TARGET_EXISTS:libdeflate::libdeflate_shared>,libdeflate::libdeflate_shared,libdeflate::libdeflate_static>
)

# TODO: Ideally we do a test to see if we need to manually link in atomics as a library
if(NOT WIN32 AND NOT APPLE AND UNIX AND LINUX)
    target_link_libraries(BetrockPlusPlus PRIVATE atomic)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Haiku")
    target_link_libraries(BetrockPlusPlus PRIVATE network)
endif()

# Debug-specific stuff
# if(CMAKE_BUILD_TYPE STREQUAL "Debug")
#     target_compile_options(BetrockPlusPlus PRIVATE
#         $<$<CXX_COMPILER_ID:GNU,Clang>:
#             # Full debug info (GDB-specific)
#             $<$<PLATFORM_ID:Linux>:-ggdb>
#             # AddressSanitizer (Linux only)
#             $<$<PLATFORM_ID:Linux>:-fsanitize=address>
#             # General debug symbols (fallback for non-Linux)
#             -g
#         >
#     )
#     target_link_options(BetrockPlusPlus PRIVATE
#         # Link AddressSanitizer runtime (Linux only)
#         $<$<AND:$<CXX_COMPILER_ID:GNU,Clang>,$<PLATFORM_ID:Linux>>:
#             -fsanitize=address
#         >
#     )
# endif()

# REMEMBER TO UPDATE THIS FOR FUTURE TOGGLES!!!!!!
target_compile_definitions(BetrockPlusPlus PRIVATE
    GENERATION_PRECISION_${GENERATION_PRECISION}
)

# TODO: We're waiting for CrashCatch to update here.
# Hopefully they make execinfo.h optional for systems that don't offer it.
if(CRASH_LOGGING AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
    include(CheckIncludeFileCXX)
    check_include_file_cxx(execinfo.h BPP_HAVE_EXECINFO)
    if(NOT BPP_HAVE_EXECINFO)
        message(WARNING "CRASH_LOGGING disabled: execinfo.h not found (musl libc?)")
        set(CRASH_LOGGING OFF)
    endif()
endif()

if(CRASH_LOGGING)
    target_compile_definitions(BetrockPlusPlus PRIVATE CRASH_LOGGING)
    if(WIN32)
        target_link_libraries(BetrockPlusPlus PRIVATE dbghelp)
    endif()
endif()

# Add some extra warnings
target_compile_options(BetrockPlusPlus PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:
        -Wall
        -Wextra
        # Implicit conversions (int32_t -> float, etc.)
        #-Wconversion
        # Sign mismatches
        #-Wsign-conversion
        # Variable shadowing
        #-Wpadded # Warn about padded structs!
        -Wshadow
        # Unused variables, params, etc.
        -Wunused
        -Wuninitialized
        # Treat warnings as errors
        #-Werror
        # Common OOP pitfall
        -Wnon-virtual-dtor
        # C-style casts
        -Wold-style-cast
        # Hides virtual functions
        -Woverloaded-virtual
        # Dereferencing null pointers
        -Wnull-dereference
        # Implicit float -> double conversions
        #-Wdouble-promotion
        # More printf/scanf format checks
        -Wformat=2
        # Warn if struct initialization is missing fields
        #-Wmissing-field-initializers
        # Multiple declarations of the same function/variable
        -Wredundant-decls
        # Comparing floating-point values for equality can be iffy
        #-Wfloat-equal
        # Warn about code that will never be executed (Clang only)
        $<$<CXX_COMPILER_ID:Clang>:-Wunreachable-code>
    >
    $<$<CXX_COMPILER_ID:MSVC>:
        # Enable most warnings / treat warnings as errors
        /W4
        # /WX
        # conversion, possible loss of data
        /w14242
        # conversion, possible loss of data (more cases)
        /w14254
        # member function does not override base
        /w14263
        # unsigned/negative mismatch
        /w14287
        # nonstandard extension used (optional strictness)
        /we4289
        # expression is always false/true
        /w14296
        # expression before comma has no effect
        /w14545
        # function call before comma missing argument
        /w14546
        # operator before comma has no effect
        /w14547
        # operator before comma has no effect
        /w14549
        # expression has no effect
        /w14555
        # pragma warning misuse
        /w14619
        # thread unsafe static init
        /w14640
        # conversion sign mismatch (closest to -Wsign-conversion)
        /w14826
        # wide string literal cast
        /w14905
        # string literal cast
        /w14906
        # illegal copy initialization
        /w14928
        # Strict standards conformance
        /permissive-
        # Suppress "nonstandard extension: nameless struct/union" (no equivalent, extension is intentional)
        /wd4201
        # Implicit conversions (no direct equiv to -Wconversion, C4244 is closest)
        /w14244
        # Sign mismatches (C4245: signed/unsigned mismatch)
        /w14245
        # Variable shadowing (C4456/C4457/C4458/C4459)
        /w14456
        /w14457
        /w14458
        /w14459
        # Unreachable code (C4702)
        /w14702
        # Missing return statement (C4715)
        /w14715
        # Destructor not virtual (C4265)
        /w14265
        # Hides virtual functions (C4264)
        /w14264
        # Uninitialized variables (C4700: uninitialized local variable used)
        /w14700
        # static analysis (catches null deref, uninitialized, etc.)
        /analyze
    >
)

# Nintendo 3DS / Switch: small terminal, and package the .elf the way the
# docs describe (BUILDING_CONSOLE.md) so there's nothing to do by hand
if(BPP_TARGET_3DS)
    # 50 columns on the top screen, so no date in the log timestamps
    target_compile_definitions(BetrockPlusPlus PRIVATE LOGGER_SHORT_TIME)
    find_program(BPP_3DSXTOOL_EXE 3dsxtool PATHS "$ENV{DEVKITPRO}/tools/bin" NO_CMAKE_FIND_ROOT_PATH)
    if(BPP_3DSXTOOL_EXE)
        add_custom_command(TARGET BetrockPlusPlus POST_BUILD
            COMMAND ${BPP_3DSXTOOL_EXE} "$<TARGET_FILE:BetrockPlusPlus>" "${CMAKE_BINARY_DIR}/BetrockPlusPlus.3dsx"
            COMMENT "Packaging BetrockPlusPlus.3dsx"
        )
    else()
        message(WARNING "3dsxtool not found; package the .3dsx by hand (BUILDING_CONSOLE.md)")
    endif()
elseif(BPP_TARGET_SWITCH)
    find_program(BPP_NACPTOOL_EXE nacptool PATHS "$ENV{DEVKITPRO}/tools/bin" NO_CMAKE_FIND_ROOT_PATH)
    find_program(BPP_ELF2NRO_EXE elf2nro PATHS "$ENV{DEVKITPRO}/tools/bin" NO_CMAKE_FIND_ROOT_PATH)
    if(BPP_NACPTOOL_EXE AND BPP_ELF2NRO_EXE)
        add_custom_command(TARGET BetrockPlusPlus POST_BUILD
            COMMAND ${BPP_NACPTOOL_EXE} --create "BetrockPlusPlus" "Pixel Brush" "${PROJECT_VERSION}" "${CMAKE_BINARY_DIR}/BetrockPlusPlus.nacp"
            COMMAND ${BPP_ELF2NRO_EXE} "$<TARGET_FILE:BetrockPlusPlus>" "${CMAKE_BINARY_DIR}/BetrockPlusPlus.nro" "--nacp=${CMAKE_BINARY_DIR}/BetrockPlusPlus.nacp"
            COMMENT "Packaging BetrockPlusPlus.nro"
        )
    else()
        message(WARNING "nacptool/elf2nro not found; package the .nro by hand (BUILDING_CONSOLE.md)")
    endif()
endif()

# Ensure all files have copyright notice
file(GLOB_RECURSE SOURCE_FILES
    "${CMAKE_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_SOURCE_DIR}/src/*.h"
)

set(REQUIRED_STRINGS
    "Copyright \\(c\\)"
    "SPDX-License-Identifier: AGPL-3.0-only"
)

foreach(FILE ${SOURCE_FILES})
    file(READ "${FILE}" CONTENTS)

    foreach(PATTERN ${REQUIRED_STRINGS})
        if(FILE MATCHES [[_shader\.h$]])
            continue()
        endif()

        string(REGEX MATCH "${PATTERN}" MATCH_RESULT "${CONTENTS}")

        if(NOT MATCH_RESULT)
            message(FATAL_ERROR
                "Missing required copyright/SPDX notice in: ${FILE}"
            )
        endif()
    endforeach()
endforeach()

# Detect CPU architecture
if(CMAKE_SYSTEM_PROCESSOR)
    set(ARCH_SUFFIX "${CMAKE_SYSTEM_PROCESSOR}")
else()
    set(ARCH_SUFFIX "unknown")
endif()

# Set up CPack for AppImage
set(CPACK_PACKAGE_NAME "BetrockPlusPlus")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Betrock++ - Beta 1.7.3-compatible Server and Client")
set(CPACK_RESOURCE_FILE_README "${CMAKE_SOURCE_DIR}/README.md")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")

# Override the default package file name
set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-${CMAKE_SYSTEM_NAME}-${ARCH_SUFFIX}")

include(CPack)
