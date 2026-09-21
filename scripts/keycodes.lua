local linux = {
    W = 25, A = 38, S = 39, D = 40,
    Q = 24, E = 26, R = 27, Y = 29, F = 41, Z = 52, C = 54,
    Up = 111, Down = 116, Left = 113, Right = 114,
    Space = 65, Escape = 9, Enter = 36, Backtick = 49,
    Shift = 50, Ctrl = 37, Tab = 23,
    F1 = 67, F2 = 68, F3 = 69, F4 = 70, F5 = 71, F6 = 72,
    F7 = 73, F8 = 74, F9 = 75, F10 = 76, F11 = 95, F12 = 96,
}

local windows = {
    W = 0x57, A = 0x41, S = 0x53, D = 0x44,
    Q = 0x51, E = 0x45, R = 0x52, Y = 0x59, F = 0x46, Z = 0x5A, C = 0x43,
    Up = 0x26, Down = 0x28, Left = 0x25, Right = 0x27,
    Space = 0x20, Escape = 0x1B, Enter = 0x0D, Backtick = 0xC0,
    Shift = 0x10, Ctrl = 0x11, Tab = 0x09,
    F1 = 0x70, F2 = 0x71, F3 = 0x72, F4 = 0x73, F5 = 0x74, F6 = 0x75,
    F7 = 0x76, F8 = 0x77, F9 = 0x78, F10 = 0x79, F11 = 0x7A, F12 = 0x7B,
}

return (PLATFORM == "windows") and windows or linux