$DX_SDK_EXE = "C:\Temp\dx_sdk.exe"

Invoke-WebRequest `
    https://download.microsoft.com/download/A/E/7/AE743F1F-632B-4809-87A9-AA1BB3458E31/DXSDK_Jun10.exe `
    -OutFile $DX_SDK_EXE

& $DX_SDK_EXE /U
