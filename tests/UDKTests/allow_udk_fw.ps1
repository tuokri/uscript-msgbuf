$UDKPath = "$PSScriptRoot\UDKTests\UDK-Lite\Binaries\Win64\UDK.exe"
New-NetFirewallRule -DisplayName "UDK.exe Outbound" -Direction Outbound -Program $UDKPath -Action Allow
New-NetFirewallRule -DisplayName "UDK.exe Inbound" -Direction Inbound -Program $UDKPath -Action Allow
