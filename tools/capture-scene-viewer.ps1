param(
    [ValidateSet("msvc-debug", "msvc-release")]
    [string]$Preset = "msvc-debug",

    [string]$Output = "",

    [string]$ResultFile = "",

    [string]$ViewerResultFile = "",

    [uint32]$Frames = 0,

    [int]$WarmupMilliseconds = 1500,

    [int]$WindowTimeoutSeconds = 30,

    [int]$CloseTimeoutSeconds = 5,

    [switch]$SkipBuild,

    [switch]$OrbitCamera,

    [string[]]$ViewerArgs = @()
)

$ErrorActionPreference = "Stop"

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")

function Resolve-ArtifactPath {
    param(
        [string]$Path,
        [string]$DefaultRelativePath
    )

    $selectedPath = $Path
    if ([string]::IsNullOrWhiteSpace($selectedPath)) {
        $selectedPath = $DefaultRelativePath
    }

    if ([System.IO.Path]::IsPathRooted($selectedPath)) {
        return [System.IO.Path]::GetFullPath($selectedPath)
    }

    return [System.IO.Path]::GetFullPath((Join-Path $repoRoot $selectedPath))
}

$outputPath = Resolve-ArtifactPath `
    -Path $Output `
    -DefaultRelativePath (Join-Path "build" (Join-Path $Preset "artifacts\scene-viewer-capture.png"))
$resultPath = Resolve-ArtifactPath `
    -Path $ResultFile `
    -DefaultRelativePath (Join-Path "build" (Join-Path $Preset "artifacts\scene-viewer-capture.json"))
$viewerResultPath = Resolve-ArtifactPath `
    -Path $ViewerResultFile `
    -DefaultRelativePath (Join-Path "build" (Join-Path $Preset "artifacts\scene-viewer-capture.viewer.json"))

foreach ($path in @($outputPath, $resultPath, $viewerResultPath)) {
    $parent = Split-Path -Parent $path
    if ($parent) {
        New-Item -ItemType Directory -Force -Path $parent | Out-Null
    }
}

if (-not ("VacSceneViewerCapture.Win32" -as [type])) {
    Add-Type -ReferencedAssemblies System.Drawing -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
using System.Text;

namespace VacSceneViewerCapture
{
    public struct Rect
    {
        public int Left;
        public int Top;
        public int Right;
        public int Bottom;
    }

    public static class Win32
    {
        public delegate bool EnumWindowsProc(IntPtr hWnd, IntPtr lParam);

        [DllImport("user32.dll")]
        public static extern bool EnumWindows(EnumWindowsProc lpEnumFunc, IntPtr lParam);

        [DllImport("user32.dll")]
        public static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint processId);

        [DllImport("user32.dll")]
        public static extern bool IsWindowVisible(IntPtr hWnd);

        [DllImport("user32.dll", CharSet = CharSet.Unicode)]
        public static extern int GetWindowText(IntPtr hWnd, StringBuilder text, int count);

        [DllImport("user32.dll")]
        public static extern bool GetWindowRect(IntPtr hWnd, out Rect rect);

        [DllImport("user32.dll")]
        public static extern bool SetForegroundWindow(IntPtr hWnd);

        [DllImport("user32.dll")]
        public static extern bool ShowWindow(IntPtr hWnd, int command);

        [DllImport("user32.dll")]
        public static extern bool PostMessage(IntPtr hWnd, uint message, IntPtr wParam, IntPtr lParam);

        [DllImport("user32.dll")]
        public static extern bool SetProcessDPIAware();

        public static string WindowTitle(IntPtr hWnd)
        {
            var title = new StringBuilder(512);
            GetWindowText(hWnd, title, title.Capacity);
            return title.ToString();
        }

        public static IntPtr FindWindowForProcess(int processId, string titleContains)
        {
            IntPtr found = IntPtr.Zero;
            EnumWindows((hWnd, lParam) =>
            {
                uint windowProcessId;
                GetWindowThreadProcessId(hWnd, out windowProcessId);
                if (windowProcessId != processId || !IsWindowVisible(hWnd))
                {
                    return true;
                }

                string title = WindowTitle(hWnd);
                if (title.Length == 0)
                {
                    return true;
                }

                if (string.IsNullOrEmpty(titleContains) ||
                    title.IndexOf(titleContains, StringComparison.OrdinalIgnoreCase) >= 0)
                {
                    found = hWnd;
                    return false;
                }

                return true;
            }, IntPtr.Zero);

            return found;
        }
    }
}
"@
}

try {
    [VacSceneViewerCapture.Win32]::SetProcessDPIAware() | Out-Null
} catch {
}

function Wait-ViewerWindow {
    param(
        [System.Diagnostics.Process]$Process,
        [int]$TimeoutSeconds
    )

    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    while ((Get-Date) -lt $deadline) {
        if ($Process.HasExited) {
            throw "vulkan_scene_viewer exited before its window was visible. ExitCode=$($Process.ExitCode)"
        }

        $window = [VacSceneViewerCapture.Win32]::FindWindowForProcess(
            $Process.Id,
            "Vulkan Action Client"
        )
        if ($window -ne [IntPtr]::Zero) {
            return $window
        }

        Start-Sleep -Milliseconds 200
    }

    throw "Timed out waiting for the Vulkan scene viewer window after $TimeoutSeconds second(s)."
}

function Save-WindowScreenshot {
    param(
        [IntPtr]$Window,
        [string]$Path
    )

    [VacSceneViewerCapture.Win32]::ShowWindow($Window, 9) | Out-Null
    [VacSceneViewerCapture.Win32]::SetForegroundWindow($Window) | Out-Null
    Start-Sleep -Milliseconds 250

    $rect = New-Object VacSceneViewerCapture.Rect
    if (-not [VacSceneViewerCapture.Win32]::GetWindowRect($Window, [ref]$rect)) {
        throw "Could not read the viewer window rectangle."
    }

    $width = $rect.Right - $rect.Left
    $height = $rect.Bottom - $rect.Top
    if ($width -le 0 -or $height -le 0) {
        throw "Viewer window rectangle is invalid: $width x $height."
    }

    $bitmap = New-Object System.Drawing.Bitmap $width, $height
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    try {
        $graphics.CopyFromScreen(
            $rect.Left,
            $rect.Top,
            0,
            0,
            (New-Object System.Drawing.Size $width, $height),
            [System.Drawing.CopyPixelOperation]::SourceCopy
        )
        $bitmap.Save($Path, [System.Drawing.Imaging.ImageFormat]::Png)
    } finally {
        $graphics.Dispose()
        $bitmap.Dispose()
    }
}

function Write-CaptureResult {
    param(
        [string]$Status,
        [string]$Message,
        [Nullable[int]]$ViewerExitCode,
        [string[]]$Diagnostics = @()
    )

    $document = [ordered]@{
        host = "capture-scene-viewer"
        status = $Status
        message = $Message
        screenshotFile = $outputPath
        viewerResultFile = $viewerResultPath
        preset = $Preset
        frames = $Frames
        warmupMilliseconds = $WarmupMilliseconds
        viewerExitCode = $ViewerExitCode
        diagnostics = $Diagnostics
    }

    $document | ConvertTo-Json -Depth 4 | Set-Content -Encoding utf8 -Path $resultPath
}

if (-not $SkipBuild) {
    . (Join-Path $PSScriptRoot "dev-shell.ps1")
    & (Join-Path $PSScriptRoot "prepare-assets.ps1")
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

    cmake --preset $Preset
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

    cmake --build --preset $Preset --target vulkan_scene_viewer
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

$buildDir = Join-Path $repoRoot (Join-Path "build" $Preset)
$viewerExe = Join-Path $buildDir "vulkan_scene_viewer.exe"
if (-not (Test-Path $viewerExe)) {
    throw "Viewer executable was not found at '$viewerExe'. Build first or omit -SkipBuild."
}

$arguments = @("--result-file", $viewerResultPath)
if ($Frames -gt 0) {
    $arguments += @("--frames", $Frames.ToString())
}
if ($OrbitCamera) {
    $arguments += "--orbit-camera"
}
$arguments += $ViewerArgs

$process = $null
$window = [IntPtr]::Zero
$diagnostics = New-Object System.Collections.Generic.List[string]

try {
    $process = Start-Process `
        -FilePath $viewerExe `
        -ArgumentList $arguments `
        -WorkingDirectory $repoRoot `
        -PassThru `
        -WindowStyle Normal

    $window = Wait-ViewerWindow -Process $process -TimeoutSeconds $WindowTimeoutSeconds
    Start-Sleep -Milliseconds $WarmupMilliseconds

    if ($process.HasExited) {
        throw "vulkan_scene_viewer exited before capture. ExitCode=$($process.ExitCode)"
    }

    Save-WindowScreenshot -Window $window -Path $outputPath
    $message = "Captured Vulkan scene viewer screenshot."
} catch {
    $message = $_.Exception.Message
    $diagnostics.Add($message)
    Write-CaptureResult -Status "error" -Message $message -ViewerExitCode $null -Diagnostics $diagnostics.ToArray()
    throw
} finally {
    if ($process -and -not $process.HasExited) {
        if ($window -ne [IntPtr]::Zero) {
            [VacSceneViewerCapture.Win32]::PostMessage($window, 0x0010, [IntPtr]::Zero, [IntPtr]::Zero) | Out-Null
        }

        if (-not $process.WaitForExit($CloseTimeoutSeconds * 1000)) {
            $diagnostics.Add("Viewer did not close within $CloseTimeoutSeconds second(s); killing process $($process.Id).")
            $process.Kill()
            $process.WaitForExit()
        }
    }
}

$exitCode = $null
if ($process) {
    $exitCode = $process.ExitCode
}

Write-CaptureResult -Status "ok" -Message $message -ViewerExitCode $exitCode -Diagnostics $diagnostics.ToArray()
Write-Host "Screenshot: $outputPath"
Write-Host "Result: $resultPath"
