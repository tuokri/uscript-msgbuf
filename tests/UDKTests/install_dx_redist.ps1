$ErrorView = 'NormalView'

$DX_WEB_SETUP = "C:\Temp\dxwebsetup.exe"

New-Item "C:\Temp\" -ItemType Directory -ErrorAction SilentlyContinue

if (Test-Path $DX_WEB_SETUP)
{
    Write-Output "'$DX_WEB_SETUP' exists"
}
else
{
    Write-Output "Downloading $DX_WEB_SETUP"
    Invoke-WebRequest `
        -Uri https://download.microsoft.com/download/1/7/1/1718CCC4-6315-4D8E-9543-8E28A4E18C4C/dxwebsetup.exe `
        -OutFile $DX_WEB_SETUP
    Write-Output "Done"
}

$DxWebSetupTemp = "C:\Temp\dx_websetup_temp\"
Start-Process -NoNewWindow -FilePath $DX_WEB_SETUP -ArgumentList "/Q", "/T:$DxWebSetupTemp"

$DX_REDIST_EXE = "C:\Temp\dx_redist.exe"

if (Test-Path $DX_REDIST_EXE)
{
    Write-Output "'$DX_REDIST_EXE' exists"
}
else
{
    Write-Output "Downloading $DX_REDIST_EXE"
    Invoke-WebRequest `
    -Uri https://download.microsoft.com/download/8/4/A/84A35BF1-DAFE-4AE8-82AF-AD2AE20B6B14/directx_Jun2010_redist.exe `
    -OutFile $DX_REDIST_EXE
    Write-Output "Done"
}

$DxRedistTemp = "C:\Temp\dx_redist_temp\"
Write-Output "Running $DX_REDIST_EXE"
Start-Process -NoNewWindow -FilePath $DX_REDIST_EXE -ArgumentList "/Q", "/T:$DxRedistTemp"

# NOTE: PowerShell does not wait for the external process to finish.
# We have to wait here manually for dx_redist.exe. This can be flaky.
# This is because the previous command starts an external program, which creates
# DXSETUP.exe, which the next command depends on.
Wait-Process -Id (Get-Process dx_redist).id

Write-Output "Running DXSETUP.exe"
Start-Process -NoNewWindow -FilePath $DxRedistTemp\DXSETUP.exe -ArgumentList "/Silent"
Wait-Process -Id (Get-Process DXSETUP).id

Write-Output "Done installing all"

Exit $LASTEXITCODE
