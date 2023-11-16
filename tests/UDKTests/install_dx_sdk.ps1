$ErrorView = 'NormalView'

$DX_SDK_EXE = "C:\Temp\dx_sdk.exe"

if (Test-Path $DX_SDK_EXE)
{
    Write-Output "'$DX_SDK_EXE' exists"
}
else
{
    Write-Output "Downloading $DX_SDK_EXE"
    Invoke-WebRequest `
    -Uri https://download.microsoft.com/download/A/E/7/AE743F1F-632B-4809-87A9-AA1BB3458E31/DXSDK_Jun10.exe `
    -OutFile $DX_SDK_EXE
    Write-Output "Done"
}

Write-Output "Running $DX_SDK_EXE"
& $DX_SDK_EXE /U
Wait-Process -Id (Get-Process dx_sdk).id

Exit $LASTEXITCODE
