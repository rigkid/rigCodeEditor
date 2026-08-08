Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing

$exe = Join-Path $PSScriptRoot "build\bin\codeEditor\codeEditor.exe"
$out = Join-Path $PSScriptRoot "img\preview.png"
$work = Split-Path $exe

if (-not (Test-Path $exe)) {
	throw "Build the hero first: cmake --build `"$PSScriptRoot\build`""
}

# Drop layout / size prefs so the hero defaults win for the pack card shot.
$ini = Join-Path $work "data\user\workspaces\imgui.ini"
$settings = Join-Path $work "data\user\rigkit_settings.json"
$workspace = Join-Path $work "data\user\workspaces\current.json"
Remove-Item $ini -Force -ErrorAction SilentlyContinue
if (Test-Path $settings) {
	$j = Get-Content $settings -Raw | ConvertFrom-Json
	if ($j.sections.'host.app') {
		$j.sections.'host.app'.'Window Width' = 1100
		$j.sections.'host.app'.'Window Height' = 720
		($j | ConvertTo-Json -Depth 10) | Set-Content $settings -Encoding UTF8
	}
}
# Clear docks so the editor is not squeezed into a leftover layout.
$wsDir = Split-Path $workspace
if (-not (Test-Path $wsDir)) { New-Item -ItemType Directory -Path $wsDir -Force | Out-Null }
@'
{
  "description": "rigCodeEditor hero",
  "name": "current",
  "windowVisibility": { "Code Editor": true },
  "imguiLayout": ""
}
'@ | Set-Content $workspace -Encoding UTF8

$proc = Start-Process -FilePath $exe -WorkingDirectory $work -PassThru
$deadline = (Get-Date).AddSeconds(25)
$hwnd = [IntPtr]::Zero
while ((Get-Date) -lt $deadline) {
	Start-Sleep -Milliseconds 400
	if ($proc.HasExited) { throw "codeEditor exited early: $($proc.ExitCode)" }
	$p = Get-Process -Id $proc.Id -ErrorAction SilentlyContinue
	if ($p -and $p.MainWindowHandle -ne [IntPtr]::Zero) {
		$hwnd = $p.MainWindowHandle
		break
	}
}
if ($hwnd -eq [IntPtr]::Zero) { throw "No main window handle after wait" }

Start-Sleep -Seconds 2

$cs = @"
using System;
using System.Runtime.InteropServices;
public static class Win32Shot {
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr hWnd, out RECT lpRect);
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr hWnd, IntPtr hdcBlt, uint nFlags);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hWnd);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left; public int Top; public int Right; public int Bottom; }
}
"@
Add-Type -TypeDefinition $cs

[void][Win32Shot]::SetForegroundWindow($hwnd)
Start-Sleep -Milliseconds 400

$rect = New-Object Win32Shot+RECT
[void][Win32Shot]::GetWindowRect($hwnd, [ref]$rect)
$w = $rect.Right - $rect.Left
$h = $rect.Bottom - $rect.Top
if ($w -lt 100 -or $h -lt 100) { throw "Bad window size ${w}x${h}" }

$bmp = New-Object System.Drawing.Bitmap $w, $h
$g = [System.Drawing.Graphics]::FromImage($bmp)
$hdc = $g.GetHdc()
$ok = [Win32Shot]::PrintWindow($hwnd, $hdc, 2)
$g.ReleaseHdc($hdc)
$g.Dispose()

if (-not $ok) {
	$g2 = [System.Drawing.Graphics]::FromImage($bmp)
	$g2.CopyFromScreen($rect.Left, $rect.Top, 0, 0, (New-Object System.Drawing.Size($w, $h)))
	$g2.Dispose()
}

$dir = Split-Path $out
if (-not (Test-Path $dir)) { New-Item -ItemType Directory -Path $dir | Out-Null }
$bmp.Save($out, [System.Drawing.Imaging.ImageFormat]::Png)
$bmp.Dispose()

Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
Write-Output "Saved $out ($w x $h)"
