$ErrorActionPreference = "Stop"

$buildDir = Join-Path "build" "dev"
$cacheFile = Join-Path $buildDir "CMakeCache.txt"

if (Test-Path $cacheFile) {
    $line = Select-String -Path $cacheFile -Pattern "^CMAKE_HOME_DIRECTORY:INTERNAL=" | Select-Object -First 1
    if ($line) {
        $cachedRaw = (($line.Line -split "=", 2)[1]).Trim()
        $cached = ([System.IO.Path]::GetFullPath($cachedRaw)).TrimEnd("\").ToLowerInvariant()
        $current = ([System.IO.Path]::GetFullPath((Resolve-Path ".").Path)).TrimEnd("\").ToLowerInvariant()

        if ($cached -ne $current) {
            Write-Host "CMake source path changed. Recreating build/dev."
            Remove-Item -Recurse -Force $buildDir
        }
    }
}

cmake --preset dev
exit $LASTEXITCODE
