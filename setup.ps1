# Setup WDK and corresponding SDK

curl.exe `
    -SL https://download.microsoft.com/download/C/D/8/CD8533F8-5324-4D30-824C-B834C5AD51F9/standalonesdk/sdksetup.exe `
    -o sdksetup_14393.exe
Start-Process .\sdksetup_14393.exe -Wait -ArgumentList '/ceip off /features + /q'
Remove-Item sdksetup_14393.exe

curl.exe `
    -SL https://download.microsoft.com/download/8/1/6/816FE939-15C7-4185-9767-42ED05524A95/wdk/wdksetup.exe `
    -o wdksetup_14393.exe
Start-Process .\wdksetup_14393.exe -Wait -ArgumentList '/ceip off /features + /q'
Remove-Item wdksetup_14393.exe

curl.exe `
    -SL https://download.microsoft.com/download/5/A/0/5A08CEF4-3EC9-494A-9578-AB687E716C12/windowssdk/winsdksetup.exe `
    -o sdksetup_17134.exe
Start-Process .\sdksetup_17134.exe -Wait -ArgumentList '/ceip off /features + /q'
Remove-Item sdksetup_17134.exe

curl.exe `
    -SL https://download.microsoft.com/download/B/5/8/B58D625D-17D6-47A8-B3D3-668670B6D1EB/wdk/wdksetup.exe `
    -o wdksetup_17134.exe
Start-Process .\wdksetup_17134.exe -Wait -ArgumentList '/ceip off /features + /q'
Remove-Item wdksetup_17134.exe

curl.exe `
    -SL https://download.microsoft.com/download/3/b/d/3bd97f81-3f5b-4922-b86d-dc5145cd6bfe/windowssdk/winsdksetup.exe `
    -o sdksetup_22621.exe
Start-Process .\sdksetup_22621.exe -Wait -ArgumentList '/ceip off /features + /q'
Remove-Item sdksetup_22621.exe

curl.exe `
    -SL https://download.microsoft.com/download/7/b/f/7bfc8dbe-00cb-47de-b856-70e696ef4f46/wdk/wdksetup.exe `
    -o wdksetup_22621.exe
Start-Process .\wdksetup_22621.exe -Wait -ArgumentList '/ceip off /features + /q'
Remove-Item wdksetup_22621.exe

# Hacks to enable ARM32 builds

$vsPath = (vswhere -property installationPath | Out-String).Trim();

$vcArm64Path = (Join-Path $vsPath "\MSBuild\Microsoft\VC\v170\Platforms\ARM64");
$vcArmPath = (Join-Path $vsPath "\MSBuild\Microsoft\VC\v170\Platforms\ARM")

if (Test-Path $vcArm64Path)
{
    Push-Location $vcArm64Path
    foreach ($file in (Get-ChildItem -Recurse $vcArm64Path -File))
    {
        $relativePath = Resolve-Path -Path $file.FullName -Relative
        $armFilePath = Join-Path $vcArmPath $relativePath
        if (Test-Path (Join-Path $vcArmPath $relativePath))
        {
            continue;
        }
        New-Item -ItemType File -Path $armFilePath -Force
        Copy-Item -Path $file.FullName -Destination $armFilePath -Force -Verbose
    }
    Pop-Location
}

$kitPath = (Get-ItemPropertyValue `
    "Registry::HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows Kits\Installed Roots" `
    -Name KitsRoot10 | `
    Out-String).Trim();

foreach ($sdkDir in (Get-ChildItem (Join-Path $kitPath "build") -Directory -Filter "10.0.*"))
{
    $sdkPath = $sdkDir.FullName

    $sdkArm64Path = (Join-Path $sdkPath "ARM64")
    $sdkArmPath = (Join-Path $sdkPath "ARM")

    if ((Test-Path $sdkArmPath) -or !(Test-Path $sdkArm64Path))
    {
        continue;
    }

    Push-Location $sdkArm64Path
    foreach ($file in (Get-ChildItem -Recurse $sdkArm64Path -File))
    {
        $relativeArm64Path = Resolve-Path -Path $file.FullName -Relative
        $relativeArmPath = $relativeArm64Path.Replace("arm64", "arm").Replace("Arm64", "Arm");
        $fileArmPath = (Join-Path $sdkArmPath $relativeArmPath)
        if (Test-Path $fileArmPath)
        {
            continue;
        }
        New-Item -ItemType File -Path $fileArmPath -Force
        $fileContent = (Get-Content $relativeArm64Path | Out-String).Replace( `
            "arm64", "arm" `
        ).Replace( `
            "Arm64", "Arm" `
        )
        Set-Content $fileArmPath -Value $fileContent
    }
    Pop-Location
}
