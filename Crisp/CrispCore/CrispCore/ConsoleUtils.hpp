#pragma once

#define NOMINMAX
#include <rlutil/rlutil.h>

namespace crisp
{
    enum class ConsoleColor
    {
        Red     = rlutil::RED,
        Brown   = rlutil::BROWN,
        Yellow  = rlutil::YELLOW,
        Green   = rlutil::GREEN,
        Cyan    = rlutil::CYAN,
        Blue    = rlutil::BLUE,
        Magenta = rlutil::MAGENTA,

        Black    = rlutil::BLACK,
        DarkGrey = rlutil::DARKGREY,
        Grey     = rlutil::GREY,
        White    = rlutil::WHITE,

        LightRed     = rlutil::LIGHTRED,
        LightGreen   = rlutil::LIGHTGREEN,
        LightCyan    = rlutil::LIGHTCYAN,
        LightBlue    = rlutil::LIGHTBLUE,
        LightMagenta = rlutil::LIGHTMAGENTA
    };

    class ConsoleColorizer
    {
    public:
        explicit ConsoleColorizer(ConsoleColor color)
        {
            rlutil::setColor(static_cast<std::underlying_type<ConsoleColor>::type>(color));
        }

        void set(ConsoleColor color)
        {
            rlutil::setColor(static_cast<std::underlying_type<ConsoleColor>::type>(color));
        }

        ~ConsoleColorizer()
        {
            rlutil::resetColor();
        }

        void restore()
        {
            rlutil::resetColor();
        }

        static void saveDefault()
        {
            rlutil::saveDefaultColor();
        }

        static void restoreDefault()
        {
            rlutil::resetColor();
        }
    };
}