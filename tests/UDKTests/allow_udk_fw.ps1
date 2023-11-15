$UDKPath = "$PSScriptRoot\UDK-Lite\Binaries\Win64\UDK.exe"
New-NetFirewallRule -DisplayName "UDK.exe Outbound UDP" -Direction Outbound -Program $UDKPath -Action Allow -Protocol UDP
New-NetFirewallRule -DisplayName "UDK.exe Outbound TCP" -Direction Outbound -Program $UDKPath -Action Allow -Protocol TCP
New-NetFirewallRule -DisplayName "UDK.exe Inbound UDP" -Direction Inbound -Program $UDKPath -Action Allow -Protocol UDP
New-NetFirewallRule -DisplayName "UDK.exe Inbound TCP" -Direction Inbound -Program $UDKPath -Action Allow -Protocol TCP
