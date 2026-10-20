param([switch]$Release)

$ErrorActionPreference = 'Stop'
Set-Location -LiteralPath $PSScriptRoot
$version = (Get-Content -LiteralPath .\VERSION -Raw).Trim()

$inc = @('-Ithird_party/freetype/include','-Ithird_party/harfbuzz')
$lib = @(
    'third_party/harfbuzz/libharfbuzz.a',
    'third_party/freetype/libfreetype.a',
    '-static-libgcc','-static-libstdc++','-Wl,-Bstatic','-lwinpthread','-Wl,-Bdynamic',
    '-lm','-lstdc++','-lshell32','-lwinmm','-ldwrite','-ld2d1',
    '-lole32','-limm32','-lgdi32','-lcomctl32','-mconsole'
)

windres -I src -O coff -o unihell_res.o src/unihell.rc
if ($LASTEXITCODE -ne 0) { throw 'Resource build failed' }

gcc -O2 -std=c99 -Wall -Wextra src/gui.c unihell_res.o -o unihell.exe @inc @lib
if ($LASTEXITCODE -ne 0) { throw 'GUI build failed' }

gcc -O2 -std=c99 -Wall -Wextra src/hell.c unihell_res.o -o unihell-cli.exe @inc @lib
if ($LASTEXITCODE -ne 0) { throw 'CLI build failed' }
Remove-Item -LiteralPath .\unihell_res.o
if (Test-Path -LiteralPath .\hell.exe) {
    Remove-Item -LiteralPath .\hell.exe
}

Write-Host 'Built unihell.exe and unihell-cli.exe'

if ($Release) {
    $dist = Join-Path $PSScriptRoot 'dist'
    $stage = Join-Path $dist "UniHELL-$version"
    $zip = Join-Path $dist "UniHELL-$version.zip"
    if (Test-Path -LiteralPath $stage) { Remove-Item -LiteralPath $stage -Recurse }
    New-Item -ItemType Directory -Path $stage -Force | Out-Null
    Copy-Item -LiteralPath .\unihell.exe,.\unihell-cli.exe,.\README.md,.\LICENSE,.\CHANGELOG.md,.\VERSION -Destination $stage
    Copy-Item -LiteralPath .\fonts -Destination $stage -Recurse
    Copy-Item -LiteralPath .\assets -Destination $stage -Recurse
    $licenses = Join-Path $stage 'THIRD_PARTY_LICENSES'
    New-Item -ItemType Directory -Path $licenses -Force | Out-Null
    Copy-Item -LiteralPath .\third_party\freetype\LICENSE.TXT -Destination (Join-Path $licenses 'FreeType-LICENSE.txt')
    Copy-Item -LiteralPath .\third_party\freetype\FTL.TXT -Destination (Join-Path $licenses 'FreeType-FTL.txt')
    Copy-Item -LiteralPath .\third_party\harfbuzz\COPYING -Destination (Join-Path $licenses 'HarfBuzz-COPYING.txt')
    Get-FileHash -LiteralPath (Join-Path $stage 'unihell.exe'),(Join-Path $stage 'unihell-cli.exe') |
        ForEach-Object { "$($_.Hash)  $([IO.Path]::GetFileName($_.Path))" } |
        Set-Content -LiteralPath (Join-Path $stage 'SHA256SUMS.txt')
    if (Test-Path -LiteralPath $zip) { Remove-Item -LiteralPath $zip }
    Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $zip
    Remove-Item -LiteralPath $stage -Recurse
    Write-Host "Release package: $zip"
}
