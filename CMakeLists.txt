#
# Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
#

AC_ADD_SCRIPT("${CMAKE_CURRENT_SOURCE_DIR}/src/BotCombatLogic.cpp")
AC_ADD_SCRIPT("${CMAKE_CURRENT_SOURCE_DIR}/src/BotCommandScript.cpp")
AC_ADD_SCRIPT("${CMAKE_CURRENT_SOURCE_DIR}/src/BotGroupScript.cpp")
AC_ADD_SCRIPT("${CMAKE_CURRENT_SOURCE_DIR}/src/BotPlayerScript.cpp")
AC_ADD_SCRIPT("${CMAKE_CURRENT_SOURCE_DIR}/src/PlayerBotMgr.cpp")
AC_ADD_SCRIPT_LOADER("PlayerBot" "${CMAKE_CURRENT_SOURCE_DIR}/src/PB_loader.cpp")