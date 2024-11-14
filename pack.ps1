param(
    [Parameter(Mandatory=$true)]
    [string]$Platform,
    [Parameter(Mandatory=$true)]
    [string]$Configuration
)

$Platform = $Platform.ToLowerInvariant();

$outDir = (Join-Path $PSScriptRoot "out\$Configuration")
mkdir -Force $outDir | Out-Null

function Get-LxMonikaProjectPlatform(
    [string]$Platform
)
{
    switch ($Platform)
    {
        "x86" { return "Win32" }
        default { return $Platform }
    }
}

function Get-LxMonikaArtifact(
    [string]$Project,
    [string]$File
)
{
    $ProjectPlatform = Get-LxMonikaProjectPlatform($Platform)
    return Join-Path "$PSScriptRoot\$Project\bin\$Configuration\$ProjectPlatform" $File
}

function Clear-Directory(
    [string]$Path
)
{
    if (Test-Path $Path)
    {
        Remove-Item -Recurse -Force $Path
    }
    New-Item -Path $Path -ItemType Directory | Out-Null
}

# Binaries

$binDir = (Join-Path $outDir "bin\$Platform")
Clear-Directory $binDir

Copy-Item `
    -Path (Get-LxMonikaArtifact "monika" "monika.exe") `
    -Destination $binDir

$binaryExts = @(".sys", ".cer")

foreach ($ext in $binaryExts)
{
    Copy-Item `
        -Path (Get-LxMonikaArtifact "lxmonika" "lxmonika$ext") `
        -Destination $binDir
}

foreach ($ext in $binaryExts)
{
    Copy-Item `
        -Path (Get-LxMonikaArtifact "lxstub" "LXCORE$ext") `
        -Destination $binDir
}

# Includes

$includeDir = (Join-Path $outDir "Include")
Clear-Directory $includeDir

Copy-Item `
    -Recurse `
    -Path "$PSScriptRoot\lxmonika\include" `
    -Destination (Join-Path $includeDir "lxmonika")

# LIBs

$libDir = (Join-Path $outDir "Lib\$Platform")
Clear-Directory $libDir

Copy-Item `
    -Path (Get-LxMonikaArtifact "lxmonika" "lxmonika.lib") `
    -Destination $libDir

Copy-Item `
    -Path (Get-LxMonikaArtifact "lxstub" "LXCORE.lib") `
    -Destination $libDir

# PDBs

$pdbDir = (Join-Path $outDir "Debug\$Platform")
Clear-Directory $pdbDir

Copy-Item `
    -Path (Get-LxMonikaArtifact "monika" "monika.pdb") `
    -Destination $pdbDir

Copy-Item `
    -Path (Get-LxMonikaArtifact "lxmonika" "lxmonika.pdb") `
    -Destination $pdbDir

Copy-Item `
    -Path (Get-LxMonikaArtifact "lxstub" "LXCORE.pdb") `
    -Destination $pdbDir
