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

# Hacks to enable ARM32 builds

$vsPath = (vswhere -property installationPath | Out-String).Trim();

$arm64Path = (Join-Path $vsPath "\MSBuild\Microsoft\VC\v170\Platforms\ARM64");
$armPath = (Join-Path $vsPath "\MSBuild\Microsoft\VC\v170\Platforms\ARM")

if (Test-Path $arm64Path)
{
    Push-Location $arm64Path
    foreach ($file in (Get-ChildItem -Recurse $arm64Path -File))
    {
        $relativePath = Resolve-Path -Path $file.FullName -Relative
        $armFilePath = Join-Path $armPath $relativePath
        if (Test-Path (Join-Path $armPath $relativePath))
        {
            continue;
        }
        New-Item -ItemType File -Path $armFilePath -Force
        Copy-Item -Path $file.FullName -Destination $armFilePath -Force -Verbose
    }
    Pop-Location
}
