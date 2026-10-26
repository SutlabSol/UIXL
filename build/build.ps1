# Build XUI Launcher from PowerShell via the MSYS2 MINGW64 toolchain.
# Usage:  .\build.ps1 [make args...]     e.g.  .\build.ps1 -j8   /  .\build.ps1 clean
param([Parameter(ValueFromRemainingArguments = $true)][string[]]$MakeArgs)
$env:MSYSTEM = "MINGW64"
$env:CHERE_INVOKING = "1"
Push-Location $PSScriptRoot
try {
    $argStr = ($MakeArgs -join ' ')
    & C:\msys64\usr\bin\bash.exe -lc "make $argStr"
    exit $LASTEXITCODE
} finally { Pop-Location }
