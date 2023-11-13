Install-Module -Name VcRedist -Force
New-Item -Path C:\Temp\VcRedist -ItemType Directory
$VcList = Get-VcList -Release "2010"
Save-VcRedist -VcList $VcList -Path C:\Temp\VcRedist
Install-VcRedist -Path C:\Temp\VcRedist -VcList $VcList -Silent
