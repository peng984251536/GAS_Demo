using System.Runtime.InteropServices;

namespace AutoClicker;

internal static class NativeMethods
{
    internal const int WH_KEYBOARD_LL = 13;
    internal const int WH_MOUSE_LL = 14;
    internal const int WM_KEYDOWN = 0x0100;
    internal const int WM_KEYUP = 0x0101;
    internal const int WM_SYSKEYDOWN = 0x0104;
    internal const int WM_SYSKEYUP = 0x0105;
    internal const int WM_LBUTTONDOWN = 0x0201;
    internal const int WM_LBUTTONUP = 0x0202;
    internal const int WM_RBUTTONDOWN = 0x0204;
    internal const int WM_RBUTTONUP = 0x0205;
    internal const uint LLMHF_INJECTED = 0x00000001;
    internal const uint LLKHF_EXTENDED = 0x00000001;
    private const uint INPUT_MOUSE = 0;
    private const uint INPUT_KEYBOARD = 1;
    private const uint MOUSEEVENTF_LEFTDOWN = 0x0002;
    private const uint MOUSEEVENTF_LEFTUP = 0x0004;
    private const uint MOUSEEVENTF_RIGHTDOWN = 0x0008;
    private const uint MOUSEEVENTF_RIGHTUP = 0x0010;
    private const uint KEYEVENTF_KEYUP = 0x0002;
    private const uint KEYEVENTF_EXTENDEDKEY = 0x0001;
    private const uint KEYEVENTF_SCANCODE = 0x0008;

    internal delegate IntPtr LowLevelKeyboardProc(int nCode, IntPtr wParam, IntPtr lParam);
    internal delegate IntPtr LowLevelMouseProc(int nCode, IntPtr wParam, IntPtr lParam);

    [StructLayout(LayoutKind.Sequential)]
    internal struct Point { internal int X; internal int Y; }

    [StructLayout(LayoutKind.Sequential)]
    internal struct MouseHookData
    {
        internal Point Point;
        internal uint MouseData;
        internal uint Flags;
        internal uint Time;
        internal UIntPtr ExtraInfo;
    }

    [StructLayout(LayoutKind.Sequential)]
    internal struct KeyboardHookData
    {
        internal uint VirtualKey;
        internal uint ScanCode;
        internal uint Flags;
        internal uint Time;
        internal UIntPtr ExtraInfo;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct Input { internal uint Type; internal InputUnion Data; }

    [StructLayout(LayoutKind.Explicit)]
    private struct InputUnion
    {
        [FieldOffset(0)] internal MouseInput Mouse;
        [FieldOffset(0)] internal KeyboardInput Keyboard;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct MouseInput
    {
        internal int X;
        internal int Y;
        internal uint MouseData;
        internal uint Flags;
        internal uint Time;
        internal UIntPtr ExtraInfo;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct KeyboardInput
    {
        internal ushort VirtualKey;
        internal ushort ScanCode;
        internal uint Flags;
        internal uint Time;
        internal UIntPtr ExtraInfo;
    }

    [DllImport("user32.dll", SetLastError = true)]
    internal static extern IntPtr SetWindowsHookEx(int idHook, LowLevelKeyboardProc callback, IntPtr module, uint threadId);

    [DllImport("user32.dll", SetLastError = true)]
    internal static extern IntPtr SetWindowsHookEx(int idHook, LowLevelMouseProc callback, IntPtr module, uint threadId);

    [DllImport("user32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    internal static extern bool UnhookWindowsHookEx(IntPtr hook);

    [DllImport("user32.dll")]
    internal static extern IntPtr CallNextHookEx(IntPtr hook, int nCode, IntPtr wParam, IntPtr lParam);

    [DllImport("kernel32.dll", CharSet = CharSet.Unicode)]
    internal static extern IntPtr GetModuleHandle(string? moduleName);

    [DllImport("user32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    internal static extern bool SetCursorPos(int x, int y);

    [DllImport("user32.dll", SetLastError = true)]
    private static extern uint SendInput(uint inputCount, Input[] inputs, int inputSize);

    internal static (bool Accepted, int ErrorCode) SendMouseButton(bool rightButton, bool isDown)
    {
        var flags = rightButton
            ? (isDown ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_RIGHTUP)
            : (isDown ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP);
        var input = new Input { Type = INPUT_MOUSE, Data = new InputUnion { Mouse = new MouseInput { Flags = flags } } };
        var accepted = SendInput(1, [input], Marshal.SizeOf<Input>()) == 1;
        return (accepted, accepted ? 0 : Marshal.GetLastWin32Error());
    }

    internal static (bool Accepted, int ErrorCode) SendKey(Keys key, int scanCode, bool extended, bool isDown)
    {
        var useScanCode = scanCode > 0;
        var flags = (isDown ? 0u : KEYEVENTF_KEYUP)
                    | (useScanCode ? KEYEVENTF_SCANCODE : 0u)
                    | (extended ? KEYEVENTF_EXTENDEDKEY : 0u);
        var input = new Input
        {
            Type = INPUT_KEYBOARD,
            Data = new InputUnion
            {
                Keyboard = new KeyboardInput
                {
                    VirtualKey = useScanCode ? (ushort)0 : (ushort)key,
                    ScanCode = (ushort)scanCode,
                    Flags = flags
                }
            }
        };
        var accepted = SendInput(1, [input], Marshal.SizeOf<Input>()) == 1;
        return (accepted, accepted ? 0 : Marshal.GetLastWin32Error());
    }
}
