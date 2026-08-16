# PlayerScript API version auto-detection.
#
# This file is included by modules/CMakeLists.txt AFTER the "modules" target
# has been defined, so we can safely use target_compile_definitions() here.
#
# Newer AzerothCore cores use OnPlayerLogin/OnPlayerUpdate in PlayerScript,
# older ones use OnLogin/OnUpdate. Detect which one the core provides and
# define PLAYERBOT_NEW_PLAYERSCRIPT so the module compiles on both versions.
set(PLAYERBOT_PLAYERSCRIPT_HEADER "${CMAKE_SOURCE_DIR}/src/server/game/Scripting/ScriptDefines/PlayerScript.h")
if(EXISTS "${PLAYERBOT_PLAYERSCRIPT_HEADER}")
    file(READ "${PLAYERBOT_PLAYERSCRIPT_HEADER}" _PLAYERBOT_PS_CONTENT)
    if(_PLAYERBOT_PS_CONTENT MATCHES "OnPlayerLogin")
        message(STATUS "mod-playerbot: detected new PlayerScript API (OnPlayerLogin/OnPlayerUpdate)")
        target_compile_definitions(modules PRIVATE PLAYERBOT_NEW_PLAYERSCRIPT)
    else()
        message(STATUS "mod-playerbot: detected legacy PlayerScript API (OnLogin/OnUpdate)")
    endif()
endif()
