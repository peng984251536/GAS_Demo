using System.Diagnostics;
using System.Media;
using System.Runtime.InteropServices;
using System.Text.Json;

namespace AutoClicker;

internal sealed class MainForm : Form
{
    private const int MaxRecordedEvents = 10_000;
    private const string RecordingFileName = "recording.json";
    private readonly ComboBox _buttonInput = new() { DropDownStyle = ComboBoxStyle.DropDownList, Width = 145 };
    private readonly NumericUpDown _intervalInput = new() { Minimum = 10, Maximum = 60_000, Value = 100, Increment = 10, Width = 145 };
    private readonly NumericUpDown _holdInput = new() { Minimum = 1, Maximum = 5_000, Value = 35, Increment = 5, Width = 145 };
    private readonly NumericUpDown _clickCountInput = new() { Minimum = 0, Maximum = 1_000_000, Value = 0, Width = 145 };
    private readonly NumericUpDown _macroRepeatInput = new() { Minimum = 0, Maximum = 1_000_000, Value = 0, Width = 110 };
    private readonly NumericUpDown _macroSpeedInput = new() { Minimum = 0.1M, Maximum = 5.0M, Value = 1.0M, Increment = 0.1M, DecimalPlaces = 1, Width = 75 };
    private readonly Label _clickStatus = new() { AutoSize = true, ForeColor = Color.DarkRed, Text = "已停止" };
    private readonly Label _macroStatus = new() { AutoSize = true, ForeColor = Color.DarkRed, Text = "未载入录制" };
    private readonly Label _clickCounter = new() { AutoSize = true, Text = "已点击：0" };
    private readonly Button _clickToggle = new() { AutoSize = true, Text = "开始连点 (F6)" };
    private readonly Button _recordToggle = new() { AutoSize = true, Text = "开始录制 (F7)" };
    private readonly Button _playToggle = new() { AutoSize = true, Text = "回放 (F8)", Enabled = false };
    private readonly Button _clearButton = new() { AutoSize = true, Text = "清空记录" };
    private readonly ListView _eventsView = new() { View = View.Details, FullRowSelect = true, GridLines = true, Dock = DockStyle.Fill };
    private readonly NativeMethods.LowLevelKeyboardProc _keyboardCallback;
    private readonly NativeMethods.LowLevelMouseProc _mouseCallback;
    private readonly List<MacroEvent> _events = [];
    private readonly HashSet<int> _keysCurrentlyDown = [];
    private readonly HashSet<Keys> _hotkeysDown = [];
    private IntPtr _keyboardHook;
    private IntPtr _mouseHook;
    private Stopwatch? _recordClock;
    private long _recordDurationMs;
    private volatile bool _recording;
    private bool _clicking;
    private bool _playing;
    private bool _closing;
    private int _clicksSent;
    private CancellationTokenSource? _clickCancellation;
    private CancellationTokenSource? _playCancellation;

    public MainForm()
    {
        Text = "游戏连点器 / 按键录制回放";
        StartPosition = FormStartPosition.CenterScreen;
        MinimumSize = new Size(620, 540);
        ClientSize = new Size(700, 570);
        Font = new Font("Microsoft YaHei UI", 9F);

        var tabs = new TabControl { Dock = DockStyle.Fill };
        var clickPage = new TabPage("连点器");
        var macroPage = new TabPage("录制回放");
        tabs.TabPages.Add(clickPage);
        tabs.TabPages.Add(macroPage);
        Controls.Add(tabs);

        BuildClickPage(clickPage);
        BuildMacroPage(macroPage);
        _clickToggle.Click += (_, _) => ToggleClicking();
        _recordToggle.Click += (_, _) => ToggleRecording();
        _playToggle.Click += async (_, _) => await TogglePlaybackAsync(3_000);
        _clearButton.Click += (_, _) => ClearRecording();

        _keyboardCallback = KeyboardHook;
        _mouseCallback = MouseHook;
        LoadRecording();
        InstallHooks();
        FormClosing += (_, _) => StopAndClose();
    }

    private void BuildClickPage(TabPage page)
    {
        _buttonInput.Items.AddRange(["鼠标左键", "鼠标右键"]);
        _buttonInput.SelectedIndex = 0;
        var layout = new TableLayoutPanel { Dock = DockStyle.Top, Padding = new Padding(28), ColumnCount = 2, RowCount = 7, Height = 330 };
        layout.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 58));
        layout.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 42));
        for (var i = 0; i < 7; i++) layout.RowStyles.Add(new RowStyle(SizeType.Absolute, 42));
        AddRow(layout, 0, "点击按键", _buttonInput);
        AddRow(layout, 1, "点击间隔（毫秒）", _intervalInput);
        AddRow(layout, 2, "按住时长（毫秒）", _holdInput);
        AddRow(layout, 3, "点击次数（0 = 无限）", _clickCountInput);
        AddRow(layout, 4, "运行状态", _clickStatus);
        AddRow(layout, 5, "点击计数", _clickCounter);
        layout.Controls.Add(_clickToggle, 0, 6);
        layout.SetColumnSpan(_clickToggle, 2);
        page.Controls.Add(layout);
        page.Controls.Add(new Label
        {
            Dock = DockStyle.Bottom,
            Height = 90,
            Padding = new Padding(28, 8, 24, 12),
            ForeColor = Color.DimGray,
            Text = "连点会在鼠标当前所在位置执行。按下与抬起分开发送，按住时长可调整。\nF6：开始 / 停止    F12：紧急停止"
        });
    }

    private void BuildMacroPage(TabPage page)
    {
        var toolbar = new FlowLayoutPanel { Dock = DockStyle.Top, Height = 48, Padding = new Padding(10, 8, 6, 4), WrapContents = false };
        toolbar.Controls.Add(_recordToggle);
        toolbar.Controls.Add(_playToggle);
        toolbar.Controls.Add(_clearButton);
        toolbar.Controls.Add(new Label { Text = "循环次数（0 = 无限）", AutoSize = true, Padding = new Padding(8, 6, 0, 0) });
        toolbar.Controls.Add(_macroRepeatInput);
        toolbar.Controls.Add(new Label { Text = "速度（倍）", AutoSize = true, Padding = new Padding(8, 6, 0, 0) });
        toolbar.Controls.Add(_macroSpeedInput);
        var statusBar = new Panel { Dock = DockStyle.Bottom, Height = 38, Padding = new Padding(12, 8, 8, 4) };
        statusBar.Controls.Add(_macroStatus);
        _eventsView.Columns.Add("时间", 95);
        _eventsView.Columns.Add("事件", 145);
        _eventsView.Columns.Add("详情", 390);
        page.Controls.Add(_eventsView);
        page.Controls.Add(statusBar);
        page.Controls.Add(toolbar);
        page.Controls.Add(new Label
        {
            Dock = DockStyle.Bottom,
            Height = 68,
            Padding = new Padding(12, 5, 12, 10),
            ForeColor = Color.DimGray,
            Text = "F7：开始 / 结束录制（有提示音）    F8：开始 / 停止回放    F12：全部停止\n点“回放”后有 3 秒切换到游戏；在游戏前台按 F8 会立即开始。"
        });
    }

    private static void AddRow(TableLayoutPanel layout, int row, string caption, Control value)
    {
        layout.Controls.Add(new Label { Text = caption, AutoSize = true, Anchor = AnchorStyles.Left }, 0, row);
        value.Anchor = AnchorStyles.Left;
        layout.Controls.Add(value, 1, row);
    }

    private void InstallHooks()
    {
        using var process = Process.GetCurrentProcess();
        using var module = process.MainModule;
        var moduleHandle = NativeMethods.GetModuleHandle(module?.ModuleName);
        _keyboardHook = NativeMethods.SetWindowsHookEx(NativeMethods.WH_KEYBOARD_LL, _keyboardCallback, moduleHandle, 0);
        _mouseHook = NativeMethods.SetWindowsHookEx(NativeMethods.WH_MOUSE_LL, _mouseCallback, moduleHandle, 0);
        if (_keyboardHook == IntPtr.Zero || _mouseHook == IntPtr.Zero)
            SetMacroStatus($"全局监听注册失败（错误 {Marshal.GetLastWin32Error()}）", Color.DarkOrange);
    }

    private IntPtr KeyboardHook(int code, IntPtr message, IntPtr data)
    {
        if (code < 0) return NativeMethods.CallNextHookEx(_keyboardHook, code, message, data);
        var keyboard = Marshal.PtrToStructure<NativeMethods.KeyboardHookData>(data);
        var key = (Keys)keyboard.VirtualKey;
        var isDown = message == (IntPtr)NativeMethods.WM_KEYDOWN || message == (IntPtr)NativeMethods.WM_SYSKEYDOWN;
        var isUp = message == (IntPtr)NativeMethods.WM_KEYUP || message == (IntPtr)NativeMethods.WM_SYSKEYUP;
        if (key is Keys.F6 or Keys.F7 or Keys.F8 or Keys.F12)
        {
            if (isUp)
            {
                _hotkeysDown.Remove(key);
                return (IntPtr)1;
            }
            if (isDown && _hotkeysDown.Add(key) && IsHandleCreated && !IsDisposed)
                BeginInvoke(() =>
                {
                    if (key == Keys.F6) ToggleClicking();
                    else if (key == Keys.F7) ToggleRecording();
                    else if (key == Keys.F8) _ = TogglePlaybackAsync();
                    else StopAll();
                });
            return (IntPtr)1;
        }

        if (_recording && !_playing && (isDown || isUp))
        {
            var virtualKey = (int)key;
            var scanCode = (int)keyboard.ScanCode;
            var extended = (keyboard.Flags & NativeMethods.LLKHF_EXTENDED) != 0;
            if (isDown && _keysCurrentlyDown.Add(virtualKey)) QueueRecordedEvent(MacroEvent.KeyEvent(virtualKey, scanCode, extended, true, RecordTime()));
            if (isUp && _keysCurrentlyDown.Remove(virtualKey)) QueueRecordedEvent(MacroEvent.KeyEvent(virtualKey, scanCode, extended, false, RecordTime()));
        }
        return NativeMethods.CallNextHookEx(_keyboardHook, code, message, data);
    }

    private IntPtr MouseHook(int code, IntPtr message, IntPtr data)
    {
        if (code >= 0 && _recording && !_playing)
        {
            var isLeft = message == (IntPtr)NativeMethods.WM_LBUTTONDOWN || message == (IntPtr)NativeMethods.WM_LBUTTONUP;
            var isRight = message == (IntPtr)NativeMethods.WM_RBUTTONDOWN || message == (IntPtr)NativeMethods.WM_RBUTTONUP;
            if (isLeft || isRight)
            {
                var mouse = Marshal.PtrToStructure<NativeMethods.MouseHookData>(data);
                var isDown = message == (IntPtr)NativeMethods.WM_LBUTTONDOWN || message == (IntPtr)NativeMethods.WM_RBUTTONDOWN;
                if ((mouse.Flags & NativeMethods.LLMHF_INJECTED) == 0)
                    QueueRecordedEvent(MacroEvent.MouseEvent(isRight, isDown, mouse.Point.X, mouse.Point.Y, RecordTime()));
            }
        }
        return NativeMethods.CallNextHookEx(_mouseHook, code, message, data);
    }

    private long RecordTime() => _recordClock?.ElapsedMilliseconds ?? 0;

    private void QueueRecordedEvent(MacroEvent macroEvent)
    {
        if (IsHandleCreated && !IsDisposed) BeginInvoke(() => AddRecordedEvent(macroEvent));
    }

    private void AddRecordedEvent(MacroEvent macroEvent)
    {
        if (!_recording || _events.Count >= MaxRecordedEvents) return;
        _events.Add(macroEvent);
        var item = new ListViewItem($"{macroEvent.TimeMs / 1000.0:F3}s");
        item.SubItems.Add(macroEvent.EventName);
        item.SubItems.Add(macroEvent.Detail);
        _eventsView.Items.Add(item);
        item.EnsureVisible();
        if (_events.Count == MaxRecordedEvents) StopRecording();
    }

    private void ToggleClicking() { if (_clicking) StopClicking(); else StartClicking(); }

    private void StartClicking()
    {
        if (_clicking || _playing || _recording) return;
        _clicksSent = 0;
        _clickCounter.Text = "已点击：0";
        _clicking = true;
        _clickCancellation = new CancellationTokenSource();
        _clickStatus.Text = "运行中";
        _clickStatus.ForeColor = Color.ForestGreen;
        _clickToggle.Text = "停止 (F6)";
        SetClickConfigEnabled(false);
        _ = RunClickLoopAsync(_clickCancellation.Token);
    }

    private async Task RunClickLoopAsync(CancellationToken cancellationToken)
    {
        var rightButton = _buttonInput.SelectedIndex == 1;
        var intervalMs = Decimal.ToInt32(_intervalInput.Value);
        var holdMs = Math.Min(Decimal.ToInt32(_holdInput.Value), intervalMs);
        var limit = Decimal.ToInt32(_clickCountInput.Value);
        try
        {
            while (!cancellationToken.IsCancellationRequested && (limit == 0 || _clicksSent < limit))
            {
                var down = NativeMethods.SendMouseButton(rightButton, true);
                if (!down.Accepted) { HandleInputError(down.ErrorCode); break; }
                try { await Task.Delay(holdMs, cancellationToken); }
                finally
                {
                    var up = NativeMethods.SendMouseButton(rightButton, false);
                    if (!up.Accepted)
                    {
                        HandleInputError(up.ErrorCode);
                        _clickCancellation?.Cancel();
                    }
                }
                cancellationToken.ThrowIfCancellationRequested();
                _clicksSent++;
                if (_clicksSent % 10 == 0 || _clicksSent == 1) UpdateClickCounter(_clicksSent);
                var gap = intervalMs - holdMs;
                if (gap > 0) await Task.Delay(gap, cancellationToken);
            }
        }
        catch (OperationCanceledException) { }
        finally
        {
            if (!_closing && IsHandleCreated) BeginInvoke(() =>
            {
                if (_clicking) StopClicking();
                UpdateClickCounter(_clicksSent);
                if (_clickStatus.Text == "运行中") SetClickStatus($"已完成（{_clicksSent} 次）", Color.DarkRed);
            });
        }
    }

    private void StopClicking()
    {
        if (!_clicking) return;
        _clickCancellation?.Cancel();
        _clicking = false;
        _clickToggle.Text = "开始连点 (F6)";
        SetClickConfigEnabled(true);
        if (_clickStatus.Text == "运行中") SetClickStatus($"已停止（{_clicksSent} 次）", Color.DarkRed);
    }

    private void ToggleRecording() { if (_recording) StopRecording(); else StartRecording(); }

    private void StartRecording()
    {
        if (_playing || _clicking) return;
        _events.Clear();
        _keysCurrentlyDown.Clear();
        _eventsView.Items.Clear();
        _recordClock = Stopwatch.StartNew();
        _recordDurationMs = 0;
        _recording = true;
        _recordToggle.Text = "结束录制 (F7)";
        _playToggle.Enabled = false;
        SetMacroStatus("录制中", Color.ForestGreen);
        SystemSounds.Asterisk.Play();
    }

    private void StopRecording()
    {
        if (!_recording) return;
        _recording = false;
        _recordDurationMs = _recordClock?.ElapsedMilliseconds ?? 0;
        _recordClock?.Stop();
        _keysCurrentlyDown.Clear();
        _recordToggle.Text = "开始录制 (F7)";
        _playToggle.Enabled = _events.Count > 0;
        SetMacroStatus($"已停止并保存（{_events.Count} 个事件）", Color.DarkRed);
        SystemSounds.Exclamation.Play();
        SaveRecording();
    }

    private async Task TogglePlaybackAsync(int startDelayMs = 0)
    {
        if (_playing) { _playCancellation?.Cancel(); return; }
        if (_recording || _clicking || _events.Count == 0) return;
        _playing = true;
        _playCancellation = new CancellationTokenSource();
        var events = _events.ToArray();
        var repeatCount = Decimal.ToInt32(_macroRepeatInput.Value);
        _playToggle.Text = "停止回放 (F8)";
        _recordToggle.Enabled = _clearButton.Enabled = _macroRepeatInput.Enabled = _macroSpeedInput.Enabled = false;
        SetMacroStatus(startDelayMs > 0 ? "3 秒后回放，请切换到游戏窗口" : "回放中（游戏需保持前台）", Color.RoyalBlue);
        try
        {
            if (startDelayMs > 0) await Task.Delay(startDelayMs, _playCancellation.Token);
            SetMacroStatus("回放中（游戏需保持前台）", Color.RoyalBlue);
            var repeat = 0;
            var speed = (double)_macroSpeedInput.Value;
            while (repeatCount == 0 || repeat < repeatCount)
            {
                long previous = 0;
                foreach (var macroEvent in events)
                {
                    var delay = macroEvent.TimeMs - previous;
                    var scaledDelay = (long)Math.Min(Math.Round(delay / speed), int.MaxValue);
                    if (scaledDelay > 0) await Task.Delay((int)scaledDelay, _playCancellation.Token);
                    _playCancellation.Token.ThrowIfCancellationRequested();
                    SendMacroEvent(macroEvent);
                    previous = macroEvent.TimeMs;
                }
                var remaining = _recordDurationMs - previous;
                var scaledRemaining = (long)Math.Min(Math.Round(remaining / speed), int.MaxValue);
                if (scaledRemaining > 0) await Task.Delay((int)scaledRemaining, _playCancellation.Token);
                repeat++;
            }
            SetMacroStatus("回放完成", Color.DarkRed);
        }
        catch (OperationCanceledException) { SetMacroStatus("回放已停止", Color.DarkRed); }
        catch (InputRejectedException ex) { SetMacroStatus(ex.Message, Color.Firebrick); }
        finally
        {
            _playing = false;
            _playCancellation?.Dispose();
            _playCancellation = null;
            _playToggle.Text = "回放 (F8)";
            _playToggle.Enabled = _events.Count > 0;
            _recordToggle.Enabled = _clearButton.Enabled = _macroRepeatInput.Enabled = _macroSpeedInput.Enabled = true;
        }
    }

    private static void SendMacroEvent(MacroEvent macroEvent)
    {
        if (macroEvent.Kind == MacroEventKind.Key)
        {
            var result = NativeMethods.SendKey((Keys)macroEvent.Key, macroEvent.ScanCode, macroEvent.Extended, macroEvent.IsDown);
            if (!result.Accepted) throw new InputRejectedException($"Windows 未接受键盘输入（错误 {result.ErrorCode}）");
        }
        else
        {
            if (!NativeMethods.SetCursorPos(macroEvent.X, macroEvent.Y))
                throw new InputRejectedException($"无法移动到录制位置（错误 {Marshal.GetLastWin32Error()}）");
            var result = NativeMethods.SendMouseButton(macroEvent.RightButton, macroEvent.IsDown);
            if (!result.Accepted) throw new InputRejectedException($"Windows 未接受鼠标输入（错误 {result.ErrorCode}）");
        }
    }

    private void HandleInputError(int errorCode)
    {
        if (!IsHandleCreated || _closing) return;
        BeginInvoke(() => SetClickStatus($"Windows 未接受输入（错误 {errorCode}）", Color.Firebrick));
    }

    private void UpdateClickCounter(int count)
    {
        if (!_closing && IsHandleCreated) BeginInvoke(() => _clickCounter.Text = $"已点击：{count}");
    }

    private void ClearRecording()
    {
        if (_recording || _playing) return;
        _events.Clear();
        _eventsView.Items.Clear();
        _playToggle.Enabled = false;
        SetMacroStatus("录制已清空", Color.DarkRed);
        try { if (File.Exists(RecordingFilePath)) File.Delete(RecordingFilePath); }
        catch (Exception ex) when (ex is IOException or UnauthorizedAccessException)
        { SetMacroStatus("无法删除 recording.json", Color.Firebrick); }
    }

    private static string RecordingFilePath => Path.Combine(AppContext.BaseDirectory, RecordingFileName);

    private void SaveRecording()
    {
        try
        {
            File.WriteAllText(RecordingFilePath, JsonSerializer.Serialize(new MacroRecording(_recordDurationMs, (double)_macroSpeedInput.Value, _events), new JsonSerializerOptions { WriteIndented = true }));
        }
        catch (Exception ex) when (ex is IOException or UnauthorizedAccessException)
        { SetMacroStatus("保存失败：请检查程序目录写入权限", Color.Firebrick); }
    }

    private void LoadRecording()
    {
        try
        {
            if (!File.Exists(RecordingFilePath)) return;
            var saved = JsonSerializer.Deserialize<MacroRecording>(File.ReadAllText(RecordingFilePath));
            if (saved?.Events is not { Count: > 0 }) return;
            _recordDurationMs = saved.DurationMs;
            if (saved.PlaybackSpeed > 0) _macroSpeedInput.Value = (decimal)Math.Clamp(saved.PlaybackSpeed, 0.1, 5.0);
            _events.AddRange(saved.Events.Take(MaxRecordedEvents));
            foreach (var macroEvent in _events)
            {
                var item = new ListViewItem($"{macroEvent.TimeMs / 1000.0:F3}s");
                item.SubItems.Add(macroEvent.EventName);
                item.SubItems.Add(macroEvent.Detail);
                _eventsView.Items.Add(item);
            }
            _playToggle.Enabled = true;
            SetMacroStatus($"已载入 recording.json（{_events.Count} 个事件）", Color.DarkGreen);
        }
        catch (Exception ex) when (ex is IOException or UnauthorizedAccessException or JsonException)
        { SetMacroStatus("录制文件无法读取", Color.Firebrick); }
    }

    private void StopAll()
    {
        StopClicking();
        if (_recording) StopRecording();
        _playCancellation?.Cancel();
    }

    private void SetClickConfigEnabled(bool enabled)
    {
        _buttonInput.Enabled = _intervalInput.Enabled = _holdInput.Enabled = _clickCountInput.Enabled = enabled;
    }

    private void SetClickStatus(string text, Color color) { _clickStatus.Text = text; _clickStatus.ForeColor = color; }
    private void SetMacroStatus(string text, Color color) { _macroStatus.Text = text; _macroStatus.ForeColor = color; }

    private void StopAndClose()
    {
        _closing = true;
        _clickCancellation?.Cancel();
        _playCancellation?.Cancel();
        if (_recording) StopRecording();
    }

    protected override void Dispose(bool disposing)
    {
        if (disposing)
        {
            _clickCancellation?.Cancel();
            _playCancellation?.Cancel();
            if (_keyboardHook != IntPtr.Zero) NativeMethods.UnhookWindowsHookEx(_keyboardHook);
            if (_mouseHook != IntPtr.Zero) NativeMethods.UnhookWindowsHookEx(_mouseHook);
            _clickCancellation?.Dispose();
            _playCancellation?.Dispose();
        }
        base.Dispose(disposing);
    }

    private enum MacroEventKind { Key, Mouse }

    private sealed record MacroEvent(MacroEventKind Kind, long TimeMs, int Key, int ScanCode, bool Extended, bool IsDown, bool RightButton, int X, int Y)
    {
        internal static MacroEvent KeyEvent(int key, int scanCode, bool extended, bool down, long time) => new(MacroEventKind.Key, time, key, scanCode, extended, down, false, 0, 0);
        internal static MacroEvent MouseEvent(bool right, bool down, int x, int y, long time) => new(MacroEventKind.Mouse, time, 0, 0, false, down, right, x, y);
        internal string EventName => Kind == MacroEventKind.Key ? (IsDown ? "键盘按下" : "键盘释放") : $"鼠标{(RightButton ? "右" : "左")}键{(IsDown ? "按下" : "释放")}";
        internal string Detail => Kind == MacroEventKind.Key ? ((Keys)Key).ToString() : $"屏幕坐标：({X}, {Y})";
    }

    private sealed record MacroRecording(long DurationMs, double PlaybackSpeed, List<MacroEvent> Events);

    private sealed class InputRejectedException(string message) : Exception(message);
}
