$DX_WEB_SETUP = "C:\Temp\dxwebsetup.exe"

Invoke-WebRequest `
    https://download.microsoft.com/download/1/7/1/1718CCC4-6315-4D8E-9543-8E28A4E18C4C/dxwebsetup.exe `
    -OutFile $DX_REDIST_EXE

& $DX_WEB_SETUP /Q

$DX_REDIST_EXE = "C:\Temp\dx_redist.exe"

Invoke-WebRequest `
    https://download.microsoft.com/download/8/4/A/84A35BF1-DAFE-4AE8-82AF-AD2AE20B6B14/directx_Jun2010_redist.exe `
    -OutFile $DX_REDIST_EXE

& $DX_REDIST_EXE /silent
