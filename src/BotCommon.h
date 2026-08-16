#ifndef _BOT_COMMON_H_
#define _BOT_COMMON_H_

#include "Chat.h"
#include "fmt/format.h"
#include <utility>

// Cross-version PSendSysMessage helper.
//
// Newer AzerothCore cores accept fmt "{}" placeholders in PSendSysMessage,
// but older cores (e.g. v4.0.0) use printf-style and would print "{}"
// literally (never substituted).
//   - new cores: pass through, they handle "{}" natively.
//   - legacy cores: pre-format with fmt::format first (legacy fmt allows
//     runtime format strings), then the printf-style call just prints it.
template <typename... Args>
inline void BotSendSysMessage(ChatHandler* handler, char const* format, Args&&... args)
{
#ifdef PLAYERBOT_NEW_PLAYERSCRIPT
    handler->PSendSysMessage(format, std::forward<Args>(args)...);
#else
    // vformat accepts runtime format strings on every fmt version.
    handler->PSendSysMessage(fmt::vformat(format, fmt::make_format_args(args...)).c_str());
#endif
}

#endif
