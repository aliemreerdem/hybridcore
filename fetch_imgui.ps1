$ErrorActionPreference = "Stop"

$imguiDir = "src\vendor\imgui"
if (-not (Test-Path $imguiDir)) {
    New-Item -ItemType Directory -Force -Path $imguiDir | Out-Null
}

$baseUrl = "https://raw.githubusercontent.com/ocornut/imgui/master/"
$files = @(
    "imgui.h", "imgui.cpp", "imgui_draw.cpp", "imgui_tables.cpp", 
    "imgui_widgets.cpp", "imconfig.h", "imgui_internal.h",
    "imstb_rectpack.h", "imstb_textedit.h", "imstb_truetype.h",
    "backends/imgui_impl_win32.h", "backends/imgui_impl_win32.cpp",
    "backends/imgui_impl_dx11.h", "backends/imgui_impl_dx11.cpp"
)

Write-Host "Downloading Dear ImGui Dependencies (Zero-Dependency Model)..."
foreach ($file in $files) {
    $url = $baseUrl + $file
    $fileName = Split-Path $file -Leaf
    $dest = Join-Path $imguiDir $fileName
    Write-Host "Fetching $fileName..."
    Invoke-WebRequest -Uri $url -OutFile $dest
}

Write-Host "ImGui Fetched Successfully into src/vendor/imgui!"
