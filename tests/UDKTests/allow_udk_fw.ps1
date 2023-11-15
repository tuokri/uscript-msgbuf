$UDKPath = "$PSScriptRoot\UDK-Lite\Binaries\Win64\UDK.exe"
$Description = "UDK.exe allowed rule."

New-NetFirewallRule -DisplayName "UDK.exe Outbound UDP" -Direction Outbound -Program $UDKPath `
    -Action Allow -Protocol UDP -Profile Any -Description $Description -Enabled True

New-NetFirewallRule -DisplayName "UDK.exe Outbound TCP" -Direction Outbound -Program $UDKPath `
    -Action Allow -Protocol TCP -Profile Any -Description $Description -Enabled True

New-NetFirewallRule -DisplayName "UDK.exe Inbound UDP" -Direction Inbound -Program $UDKPath `
    -Action Allow -Protocol UDP -Profile Any -Description $Description -Enabled True

New-NetFirewallRule -DisplayName "UDK.exe Inbound TCP" -Direction Inbound -Program $UDKPath `
    -Action Allow -Protocol TCP -Profile Any -Description $Description -Enabled True
