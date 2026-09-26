using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text.Json;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace UrsfAutoClicker
{
    public partial class Form1 : Form
    {
        [DllImport("user32.dll")]
        private static extern short GetAsyncKeyState(int vKey);

        [DllImport("user32.dll")]
        private static extern IntPtr GetForegroundWindow();

        [DllImport("user32.dll")]
        private static extern uint GetWindowThreadProcessId(
            IntPtr hWnd,
            out uint processId);

        [DllImport("user32.dll")]
        private static extern void mouse_event(
            uint dwFlags,
            uint dx,
            uint dy,
            uint dwData,
            UIntPtr dwExtraInfo);

        [DllImport("user32.dll", SetLastError = true)]
        private static extern bool DestroyIcon(IntPtr hIcon);

        private const uint MOUSEEVENTF_LEFTDOWN = 0x0002;
        private const uint MOUSEEVENTF_LEFTUP = 0x0004;
        private const uint MOUSEEVENTF_RIGHTDOWN = 0x0008;
        private const uint MOUSEEVENTF_RIGHTUP = 0x0010;
        private const uint MOUSEEVENTF_MIDDLEDOWN = 0x0020;
        private const uint MOUSEEVENTF_MIDDLEUP = 0x0040;

        private readonly System.Windows.Forms.Timer clickTimer = new();
        private readonly System.Windows.Forms.Timer hotkeyTimer = new();

        private Label activationLabel = null!;
        private Label statusLabel = null!;
        private Label appStatusLabel = null!;

        private Button selectButton = null!;
        private Button chooseAppsButton = null!;

        private RadioButton holdRadio = null!;
        private RadioButton toggleRadio = null!;

        private RadioButton leftRadio = null!;
        private RadioButton rightRadio = null!;
        private RadioButton middleRadio = null!;

        private NumericUpDown cpsBox = null!;
        private NumericUpDown limitBox = null!;
        private NumericUpDown dutyBox = null!;

        private CheckBox randomCheck = null!;
        private CheckBox limitCheck = null!;

        private Icon? iconOn;
        private Icon? iconOff;

        private int activationKey = (int)Keys.F6;

        private bool selectingKey = false;
        private long keySelectionStart = 0;

        private bool clickerEnabled = false;
        private bool previousActivationState = false;

        private bool clickInProgress = false;
        private bool blockedUntilRelease = false;

        private int clickCount = 0;

        private readonly Random random = new();

        private readonly HashSet<string> selectedApps =
            new(StringComparer.OrdinalIgnoreCase);

        private readonly string configFolder =
            Path.Combine(
                Environment.GetFolderPath(Environment.SpecialFolder.MyDocuments),
                "UrsfAutoClicker");

        private string ConfigPath =>
            Path.Combine(configFolder, "config.json");

        public Form1()
        {
            InitializeComponent();

            BuildInterface();
            LoadStatusIcons();
            LoadConfig();

            clickTimer.Tick += ClickTimer_Tick;

            hotkeyTimer.Interval = 10;
            hotkeyTimer.Tick += HotkeyTimer_Tick;
            hotkeyTimer.Start();

            UpdateClickInterval();
        }

        private void BuildInterface()
        {
            Text = "URSF AutoClicker";

            ClientSize = new Size(520, 545);

            FormBorderStyle = FormBorderStyle.FixedSingle;
            MaximizeBox = false;
            StartPosition = FormStartPosition.CenterScreen;

            Font = new Font("Segoe UI", 9F);

            GroupBox activationGroup = new()
            {
                Text = "Activation",
                Location = new Point(10, 10),
                Size = new Size(500, 145)
            };

            Label keyText = new()
            {
                Text = "Activation Key:",
                Location = new Point(15, 31),
                AutoSize = true
            };

            activationLabel = new Label()
            {
                Text = "F6",
                BorderStyle = BorderStyle.FixedSingle,
                TextAlign = ContentAlignment.MiddleCenter,
                Location = new Point(130, 24),
                Size = new Size(180, 30),
                Font = new Font("Segoe UI", 9F, FontStyle.Bold)
            };

            selectButton = new Button()
            {
                Text = "SELECT...",
                Location = new Point(325, 24),
                Size = new Size(155, 30)
            };

            selectButton.Click += SelectButton_Click;

            Label modeText = new()
            {
                Text = "Activation Mode:",
                Location = new Point(15, 72),
                AutoSize = true
            };

            holdRadio = new RadioButton()
            {
                Text = "Hold",
                Location = new Point(130, 69),
                AutoSize = true
            };

            toggleRadio = new RadioButton()
            {
                Text = "Toggle",
                Location = new Point(210, 69),
                AutoSize = true,
                Checked = true
            };

            chooseAppsButton = new Button()
            {
                Text = "CHOOSE APPS",
                Location = new Point(325, 66),
                Size = new Size(155, 30)
            };

            chooseAppsButton.Click += ChooseAppsButton_Click;

            appStatusLabel = new Label()
            {
                Text = "Applications: All",
                Location = new Point(15, 111),
                Size = new Size(465, 22),
                ForeColor = Color.DimGray
            };

            activationGroup.Controls.AddRange(
                new Control[]
                {
                    keyText,
                    activationLabel,
                    selectButton,
                    modeText,
                    holdRadio,
                    toggleRadio,
                    chooseAppsButton,
                    appStatusLabel
                });

            GroupBox clicksGroup = new()
            {
                Text = "Clicks",
                Location = new Point(10, 165),
                Size = new Size(500, 110)
            };

            leftRadio = new RadioButton()
            {
                Text = "Left Mouse Button",
                Location = new Point(20, 35),
                AutoSize = true,
                Checked = true
            };

            rightRadio = new RadioButton()
            {
                Text = "Right Mouse Button",
                Location = new Point(180, 35),
                AutoSize = true
            };

            middleRadio = new RadioButton()
            {
                Text = "Middle Mouse Button",
                Location = new Point(350, 35),
                AutoSize = true
            };

            Label clickInfo = new()
            {
                Text = "Choose which mouse button should be clicked.",
                Location = new Point(20, 72),
                AutoSize = true,
                ForeColor = Color.DimGray
            };

            clicksGroup.Controls.AddRange(
                new Control[]
                {
                    leftRadio,
                    rightRadio,
                    middleRadio,
                    clickInfo
                });

            GroupBox rateGroup = new()
            {
                Text = "Click Rate",
                Location = new Point(10, 285),
                Size = new Size(270, 185)
            };

            Label cpsText = new()
            {
                Text = "Clicks per second:",
                Location = new Point(15, 32),
                AutoSize = true
            };

            cpsBox = new NumericUpDown()
            {
                Location = new Point(140, 28),
                Size = new Size(100, 25),
                Minimum = 1,
                Maximum = 100,
                DecimalPlaces = 1,
                Increment = 1,
                Value = 20
            };

            cpsBox.ValueChanged += (_, _) =>
            {
                UpdateClickInterval();
            };

            randomCheck = new CheckBox()
            {
                Text = "Randomize timing",
                Location = new Point(15, 68),
                AutoSize = true
            };

            randomCheck.CheckedChanged += (_, _) =>
            {
                UpdateClickInterval();
            };

            Label dutyText = new()
            {
                Text = "Click duty cycle:",
                Location = new Point(15, 107),
                AutoSize = true
            };

            dutyBox = new NumericUpDown()
            {
                Location = new Point(140, 103),
                Size = new Size(75, 25),
                Minimum = 5,
                Maximum = 95,
                Value = 25
            };

            Label percentText = new()
            {
                Text = "%",
                Location = new Point(220, 107),
                AutoSize = true
            };

            Label cpsHint = new()
            {
                Text = "1–100 CPS",
                Location = new Point(15, 145),
                AutoSize = true,
                ForeColor = Color.DimGray
            };

            rateGroup.Controls.AddRange(
                new Control[]
                {
                    cpsText,
                    cpsBox,
                    randomCheck,
                    dutyText,
                    dutyBox,
                    percentText,
                    cpsHint
                });

            GroupBox limitGroup = new()
            {
                Text = "Click Limit",
                Location = new Point(290, 285),
                Size = new Size(220, 185)
            };

            limitCheck = new CheckBox()
            {
                Text = "Enable Click Limit",
                Location = new Point(15, 32),
                AutoSize = true
            };

            limitBox = new NumericUpDown()
            {
                Location = new Point(15, 68),
                Size = new Size(180, 25),
                Minimum = 1,
                Maximum = 1000000,
                Value = 100,
                Enabled = false
            };

            limitCheck.CheckedChanged += (_, _) =>
            {
                limitBox.Enabled = limitCheck.Checked;
            };

            Label maximumText = new()
            {
                Text = "Maximum number of clicks",
                Location = new Point(15, 105),
                AutoSize = true,
                ForeColor = Color.DimGray
            };

            limitGroup.Controls.AddRange(
                new Control[]
                {
                    limitCheck,
                    limitBox,
                    maximumText
                });

            statusLabel = new Label()
            {
                Text = "● Stopped — Press F6",
                Location = new Point(15, 495),
                Size = new Size(490, 30),
                Font = new Font("Segoe UI", 9F, FontStyle.Bold)
            };

            Controls.AddRange(
                new Control[]
                {
                    activationGroup,
                    clicksGroup,
                    rateGroup,
                    limitGroup,
                    statusLabel
                });
        }

        // =========================================================
        // STATUS ICONS
        // =========================================================

        private void LoadStatusIcons()
        {
            try
            {
                iconOff = CreateIconFromBitmap(Properties.Resources.off);
                iconOn = CreateIconFromBitmap(Properties.Resources.on);

                if (iconOff != null)
                    Icon = iconOff;
            }
            catch
            {
                // İkonlar yüklenemezse program normal çalışmaya devam etsin.
            }
        }

        private static Icon CreateIconFromBitmap(Bitmap bitmap)
        {
            IntPtr handle = bitmap.GetHicon();

            try
            {
                using Icon tempIcon = Icon.FromHandle(handle);

                return (Icon)tempIcon.Clone();
            }
            finally
            {
                DestroyIcon(handle);
            }
        }

        private void SetRunningIcon(bool running)
        {
            Icon? target =
                running ? iconOn : iconOff;

            if (target != null)
            {
                Icon = target;
            }
        }

        // =========================================================
        // CONFIG
        // =========================================================

        private sealed class AppConfig
        {
            public int ActivationKey { get; set; } = (int)Keys.F6;
            public bool HoldMode { get; set; } = false;
            public string MouseButton { get; set; } = "Left";
            public decimal CPS { get; set; } = 20;
            public bool Randomize { get; set; } = false;
            public decimal DutyCycle { get; set; } = 25;
            public bool ClickLimitEnabled { get; set; } = false;
            public decimal ClickLimit { get; set; } = 100;
            public List<string> SelectedApps { get; set; } = new();
        }

        private void SaveConfig()
        {
            try
            {
                Directory.CreateDirectory(configFolder);

                AppConfig config = new()
                {
                    ActivationKey = activationKey,

                    HoldMode = holdRadio.Checked,

                    MouseButton =
                        leftRadio.Checked ? "Left" :
                        rightRadio.Checked ? "Right" :
                        "Middle",

                    CPS = cpsBox.Value,

                    Randomize = randomCheck.Checked,

                    DutyCycle = dutyBox.Value,

                    ClickLimitEnabled = limitCheck.Checked,

                    ClickLimit = limitBox.Value,

                    SelectedApps = selectedApps.ToList()
                };

                string json =
                    JsonSerializer.Serialize(
                        config,
                        new JsonSerializerOptions
                        {
                            WriteIndented = true
                        });

                File.WriteAllText(
                    ConfigPath,
                    json);
            }
            catch
            {
            }
        }

        private void LoadConfig()
        {
            try
            {
                if (!File.Exists(ConfigPath))
                    return;

                string json =
                    File.ReadAllText(ConfigPath);

                AppConfig? config =
                    JsonSerializer.Deserialize<AppConfig>(json);

                if (config == null)
                    return;

                activationKey =
                    config.ActivationKey;

                activationLabel.Text =
                    GetKeyName(activationKey);

                holdRadio.Checked =
                    config.HoldMode;

                toggleRadio.Checked =
                    !config.HoldMode;

                leftRadio.Checked =
                    config.MouseButton == "Left";

                rightRadio.Checked =
                    config.MouseButton == "Right";

                middleRadio.Checked =
                    config.MouseButton == "Middle";

                cpsBox.Value =
                    Math.Clamp(
                        config.CPS,
                        cpsBox.Minimum,
                        cpsBox.Maximum);

                randomCheck.Checked =
                    config.Randomize;

                dutyBox.Value =
                    Math.Clamp(
                        config.DutyCycle,
                        dutyBox.Minimum,
                        dutyBox.Maximum);

                limitCheck.Checked =
                    config.ClickLimitEnabled;

                limitBox.Value =
                    Math.Clamp(
                        config.ClickLimit,
                        limitBox.Minimum,
                        limitBox.Maximum);

                selectedApps.Clear();

                if (config.SelectedApps != null)
                {
                    foreach (string app in config.SelectedApps)
                    {
                        selectedApps.Add(app);
                    }
                }

                appStatusLabel.Text =
                    selectedApps.Count == 0
                        ? "Applications: All"
                        : "Applications: "
                          + string.Join(", ", selectedApps);

                statusLabel.Text =
                    holdRadio.Checked
                        ? $"● Ready — Hold {GetKeyName(activationKey)}"
                        : $"● Stopped — Press {GetKeyName(activationKey)}";

                UpdateClickInterval();
            }
            catch
            {
            }
        }

        // =========================================================
        // ACTIVATION KEY SELECTION
        // =========================================================

        private void SelectButton_Click(
            object? sender,
            EventArgs e)
        {
            selectingKey = true;

            keySelectionStart =
                Environment.TickCount64;

            activationLabel.Text =
                "Press a key...";

            selectButton.Enabled = false;

            statusLabel.Text =
                "● Waiting for keyboard / Mouse4 / Mouse5...";
        }

        private void CheckForNewActivationKey()
        {
            if (Environment.TickCount64 -
                keySelectionStart < 250)
            {
                return;
            }

            if (IsKeyDown((int)Keys.Escape))
            {
                selectingKey = false;
                selectButton.Enabled = true;

                activationLabel.Text =
                    GetKeyName(activationKey);

                statusLabel.Text =
                    $"● Selection cancelled — {GetKeyName(activationKey)}";

                return;
            }

            if (IsKeyDown((int)Keys.XButton1))
            {
                SetActivationKey(
                    (int)Keys.XButton1);

                return;
            }

            if (IsKeyDown((int)Keys.XButton2))
            {
                SetActivationKey(
                    (int)Keys.XButton2);

                return;
            }

            for (int key = 8; key <= 254; key++)
            {
                if (key ==
                    (int)Keys.LButton ||
                    key ==
                    (int)Keys.RButton ||
                    key ==
                    (int)Keys.MButton ||
                    key ==
                    (int)Keys.XButton1 ||
                    key ==
                    (int)Keys.XButton2)
                {
                    continue;
                }

                if (IsKeyDown(key))
                {
                    SetActivationKey(key);
                    return;
                }
            }
        }

        private void SetActivationKey(int key)
        {
            activationKey = key;

            selectingKey = false;

            selectButton.Enabled = true;

            activationLabel.Text =
                GetKeyName(key);

            previousActivationState = true;

            statusLabel.Text =
                $"● Activation key: {GetKeyName(key)}";
        }

        private string GetKeyName(int key)
        {
            if (key == (int)Keys.XButton1)
                return "Mouse4 / XButton1";

            if (key == (int)Keys.XButton2)
                return "Mouse5 / XButton2";

            return ((Keys)key).ToString();
        }

        private bool IsKeyDown(int key)
        {
            return
                (GetAsyncKeyState(key) & 0x8000)
                != 0;
        }

        // =========================================================
        // HOTKEY
        // =========================================================

        private void HotkeyTimer_Tick(
            object? sender,
            EventArgs e)
        {
            if (selectingKey)
            {
                CheckForNewActivationKey();
                return;
            }

            bool activationDown =
                IsKeyDown(activationKey);

            if (blockedUntilRelease)
            {
                if (!activationDown)
                {
                    blockedUntilRelease = false;
                }

                previousActivationState =
                    activationDown;

                return;
            }

            if (holdRadio.Checked)
            {
                if (activationDown)
                {
                    if (!clickerEnabled)
                    {
                        StartClicker();
                    }
                }
                else
                {
                    if (clickerEnabled)
                    {
                        StopClicker();
                    }

                    statusLabel.Text =
                        $"● Ready — Hold {GetKeyName(activationKey)}";
                }
            }
            else
            {
                if (activationDown &&
                    !previousActivationState)
                {
                    if (clickerEnabled)
                    {
                        StopClicker();

                        statusLabel.Text =
                            $"● Stopped — Press {GetKeyName(activationKey)}";
                    }
                    else
                    {
                        StartClicker();
                    }
                }
            }

            previousActivationState =
                activationDown;
        }

        private void StartClicker()
        {
            clickerEnabled = true;
            clickCount = 0;

            SetRunningIcon(true);

            UpdateClickInterval();

            clickTimer.Start();

            statusLabel.Text =
                $"● Clicking — {cpsBox.Value:0.0} CPS";
        }

        private void StopClicker()
        {
            clickerEnabled = false;

            clickTimer.Stop();

            SetRunningIcon(false);
        }

        // =========================================================
        // CLICK TIMER
        // =========================================================

        private async void ClickTimer_Tick(
            object? sender,
            EventArgs e)
        {
            if (!clickerEnabled)
                return;

            if (clickInProgress)
                return;

            if (!IsCurrentApplicationAllowed())
            {
                statusLabel.Text =
                    "● Paused — Application not selected";

                return;
            }

            clickInProgress = true;

            try
            {
                UpdateClickInterval();

                int totalInterval =
                    clickTimer.Interval;

                double duty =
                    (double)dutyBox.Value / 100.0;

                int downTime =
                    Math.Max(
                        1,
                        (int)(totalInterval * duty));

                MouseDown();

                await Task.Delay(downTime);

                MouseUp();

                clickCount++;

                statusLabel.Text =
                    $"● Clicking — {cpsBox.Value:0.0} CPS | {clickCount} clicks";

                if (limitCheck.Checked &&
                    clickCount >=
                    (int)limitBox.Value)
                {
                    StopClicker();

                    if (holdRadio.Checked)
                    {
                        blockedUntilRelease = true;
                    }

                    statusLabel.Text =
                        $"● Finished — {clickCount} clicks";
                }
            }
            finally
            {
                clickInProgress = false;
            }
        }

        private void UpdateClickInterval()
        {
            if (cpsBox == null)
                return;

            double cps =
                (double)cpsBox.Value;

            double interval =
                1000.0 / cps;

            if (randomCheck != null &&
                randomCheck.Checked)
            {
                double variation =
                    (random.NextDouble() * 0.30)
                    - 0.15;

                interval *=
                    1.0 + variation;
            }

            int ms =
                Math.Max(
                    1,
                    (int)Math.Round(interval));

            clickTimer.Interval = ms;
        }

        // =========================================================
        // MOUSE
        // =========================================================

        private void MouseDown()
        {
            if (leftRadio.Checked)
            {
                mouse_event(
                    MOUSEEVENTF_LEFTDOWN,
                    0,
                    0,
                    0,
                    UIntPtr.Zero);
            }
            else if (rightRadio.Checked)
            {
                mouse_event(
                    MOUSEEVENTF_RIGHTDOWN,
                    0,
                    0,
                    0,
                    UIntPtr.Zero);
            }
            else
            {
                mouse_event(
                    MOUSEEVENTF_MIDDLEDOWN,
                    0,
                    0,
                    0,
                    UIntPtr.Zero);
            }
        }

        private void MouseUp()
        {
            if (leftRadio.Checked)
            {
                mouse_event(
                    MOUSEEVENTF_LEFTUP,
                    0,
                    0,
                    0,
                    UIntPtr.Zero);
            }
            else if (rightRadio.Checked)
            {
                mouse_event(
                    MOUSEEVENTF_RIGHTUP,
                    0,
                    0,
                    0,
                    UIntPtr.Zero);
            }
            else
            {
                mouse_event(
                    MOUSEEVENTF_MIDDLEUP,
                    0,
                    0,
                    0,
                    UIntPtr.Zero);
            }
        }

        // =========================================================
        // APP FILTER
        // =========================================================

        private void ChooseAppsButton_Click(
            object? sender,
            EventArgs e)
        {
            using Form dialog =
                new Form();

            dialog.Text =
                "Choose Applications";

            dialog.ClientSize =
                new Size(500, 420);

            dialog.StartPosition =
                FormStartPosition.CenterParent;

            dialog.FormBorderStyle =
                FormBorderStyle.FixedDialog;

            dialog.MaximizeBox = false;
            dialog.MinimizeBox = false;

            Label info = new()
            {
                Text =
                    "Select applications where the auto clicker is allowed.\n" +
                    "Leave everything unselected to allow all applications.",

                Location =
                    new Point(15, 15),

                Size =
                    new Size(470, 45)
            };

            ListBox list = new()
            {
                Location =
                    new Point(15, 65),

                Size =
                    new Size(470, 285),

                SelectionMode =
                    SelectionMode.MultiExtended
            };

            List<AppItem> apps =
                new();

            foreach (
                Process process
                in Process
                    .GetProcesses()
                    .OrderBy(
                        p =>
                        {
                            try
                            {
                                return p.ProcessName;
                            }
                            catch
                            {
                                return "";
                            }
                        }))
            {
                try
                {
                    if (string.IsNullOrWhiteSpace(
                        process.MainWindowTitle))
                    {
                        continue;
                    }

                    if (apps.Any(
                        x =>
                        x.ProcessName.Equals(
                            process.ProcessName,
                            StringComparison.OrdinalIgnoreCase)))
                    {
                        continue;
                    }

                    AppItem item =
                        new(
                            process.ProcessName,
                            process.MainWindowTitle);

                    apps.Add(item);
                    list.Items.Add(item);

                    if (selectedApps.Contains(
                        process.ProcessName))
                    {
                        list.SetSelected(
                            list.Items.Count - 1,
                            true);
                    }
                }
                catch
                {
                }
            }

            Button allButton = new()
            {
                Text = "Allow All",
                Location =
                    new Point(15, 365),

                Size =
                    new Size(100, 32)
            };

            Button okButton = new()
            {
                Text = "OK",

                Location =
                    new Point(275, 365),

                Size =
                    new Size(100, 32),

                DialogResult =
                    DialogResult.OK
            };

            Button cancelButton = new()
            {
                Text = "Cancel",

                Location =
                    new Point(385, 365),

                Size =
                    new Size(100, 32),

                DialogResult =
                    DialogResult.Cancel
            };

            allButton.Click += (_, _) =>
            {
                list.ClearSelected();
            };

            dialog.Controls.AddRange(
                new Control[]
                {
                    info,
                    list,
                    allButton,
                    okButton,
                    cancelButton
                });

            dialog.AcceptButton =
                okButton;

            dialog.CancelButton =
                cancelButton;

            if (dialog.ShowDialog(this)
                != DialogResult.OK)
            {
                return;
            }

            selectedApps.Clear();

            foreach (
                AppItem item
                in list.SelectedItems)
            {
                selectedApps.Add(
                    item.ProcessName);
            }

            if (selectedApps.Count == 0)
            {
                appStatusLabel.Text =
                    "Applications: All";
            }
            else
            {
                appStatusLabel.Text =
                    "Applications: "
                    + string.Join(
                        ", ",
                        selectedApps);
            }
        }

        private bool IsCurrentApplicationAllowed()
        {
            if (selectedApps.Count == 0)
                return true;

            try
            {
                IntPtr window =
                    GetForegroundWindow();

                if (window == IntPtr.Zero)
                    return false;

                GetWindowThreadProcessId(
                    window,
                    out uint pid);

                Process process =
                    Process.GetProcessById(
                        (int)pid);

                return selectedApps.Contains(
                    process.ProcessName);
            }
            catch
            {
                return false;
            }
        }

        private sealed class AppItem
        {
            public string ProcessName { get; }

            private string WindowTitle { get; }

            public AppItem(
                string processName,
                string windowTitle)
            {
                ProcessName =
                    processName;

                WindowTitle =
                    windowTitle;
            }

            public override string ToString()
            {
                return
                    $"{ProcessName} — {WindowTitle}";
            }
        }

        // =========================================================
        // EXIT
        // =========================================================

        protected override void OnFormClosing(
            FormClosingEventArgs e)
        {
            SaveConfig();

            clickTimer.Stop();
            hotkeyTimer.Stop();

            try
            {
                MouseUp();
            }
            catch
            {
            }

            iconOn?.Dispose();
            iconOff?.Dispose();

            base.OnFormClosing(e);
        }
    }
}