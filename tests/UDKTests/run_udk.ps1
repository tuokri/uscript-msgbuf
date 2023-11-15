Start-Process -NoNewWindow -FilePath $PSScriptRoot\UDK-Lite\Binaries\Win64\UDK.com `
    -ArgumentList "make", "-useunpublished"

# TODO: this can list all event logs for "UDK".
Get-WinEvent -ListLog * |
        ForEach-Object -parallel { Get-WinEvent @{ logname = $_.logname } -ea 0 } `
            | Where-Object message -Match UDK | Format-Table -Wrap
