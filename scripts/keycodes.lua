local linux = {
    W = 25, A = 38, S = 39, D = 40,
    Up = 111, Down = 116, Left = 113, Right = 114,
    Space = 65, Escape = 9, Enter = 36, Backtick = 49,
    Shift = 50, Ctrl = 37, F11 = 95,
}

local windows = {
    W = 0x57, A = 0x41, S = 0x53, D = 0x44,
    Up = 0x26, Down = 0x28, Left = 0x25, Right = 0x27,
    Space = 0x20, Escape = 0x1B, Enter = 0x0D, Backtick = 0xC0,
    Shift = 0x10, Ctrl = 0x11, F11 = 0x7A,
}

return (PLATFORM == "windows") and windows or linux