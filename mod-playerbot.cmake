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

# ---------------------------------------------------------------------------
# Dynamic git revision
#
# Runs `git rev-parse --short HEAD` at (re)configure time so /bot shows the
# exact commit this binary was built from. To keep it accurate, re-run cmake
# before rebuilding:
#   git commit ... && cmake . && make worldserver && restart
#
# RULE (user): every restart MUST be preceded by commit + reconfigure + rebuild
# so the version number always reflects the running code.
#
# NOTE: must use CMAKE_CURRENT_LIST_DIR (this .cmake's dir = the module dir),
# NOT CMAKE_CURRENT_SOURCE_DIR which points at the AzerothCore repo root and
# would inject the core's commit instead of the module's.
# ---------------------------------------------------------------------------
execute_process(
    COMMAND git -C "${CMAKE_CURRENT_LIST_DIR}" rev-parse --short HEAD
    OUTPUT_VARIABLE PLAYERBOT_REV
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)
if(NOT PLAYERBOT_REV)
    set(PLAYERBOT_REV "unknown")
endif()
message(STATUS "mod-playerbot: revision = ${PLAYERBOT_REV}")
target_compile_definitions(modules PRIVATE PLAYERBOT_REV="${PLAYERBOT_REV}")
