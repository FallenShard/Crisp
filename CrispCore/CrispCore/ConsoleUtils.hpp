#pragma once

#define NOMINMAX
#pragma warning(push)
#pragma warning(disable:6031)
#include <rlutil/rlutil.h>
#pragma warning(pop)

#include <stack>

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
        static std::stack<ConsoleColor> colorStack;

        inline explicit ConsoleColorizer(ConsoleColor color)
        {
            if (colorStack.empty())
                saveDefault();

            colorStack.push(color);
            rlutil::setColor(static_cast<std::underlying_type<ConsoleColor>::type>(color));
        }

        inline ~ConsoleColorizer()
        {
            colorStack.pop();
            if (!colorStack.empty())
                rlutil::setColor(static_cast<std::underlying_type<ConsoleColor>::type>(colorStack.top()));
            else
                restoreDefault();
        }

        inline void set(ConsoleColor color)
        {
            colorStack.top() = color;
            rlutil::setColor(static_cast<std::underlying_type<ConsoleColor>::type>(color));
        }

        inline void restore()
        {
            rlutil::resetColor();
        }

        inline static void saveDefault()
        {
            rlutil::saveDefaultColor();
        }

        inline static void restoreDefault()
        {
            rlutil::resetColor();
        }
    };
}