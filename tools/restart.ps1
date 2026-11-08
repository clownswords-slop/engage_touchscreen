# restart.ps1 [release]: stop Ryujinx, install the debug (or release) build, boot Engage, wait for the mod's init log line
param([string]$kind="debug")
$root="E:\emulators\Ryujinx"; $mod="$env:APPDATA\Ryujinx\mods\contents\0100a6301214e000\EngageTouchMod\exefs"
Set-Content -Path "$env:APPDATA\Ryujinx\sdcard\engagetouch\cmd.txt" -Value "2`ninfo" -Encoding ascii
Get-Process Ryujinx -ErrorAction SilentlyContinue | Stop-Process; Start-Sleep 3
$src= if($kind -eq "release"){ "subsdk9" } else { "subsdk9_dbg" }
Copy-Item "$root\claude_engage_touchmod\build\$src" "$mod\subsdk9" -Force
Copy-Item "$root\claude_engage_touchmod\build\main.npdm" "$mod\main.npdm" -Force
$t0=Get-Date
Start-Process -FilePath "$root\Ryujinx.exe" -ArgumentList "`"$root\Fire Emblem Engage [0100A6301214E000][v0].nsp`"" -WorkingDirectory $root
for($i=0;$i -lt 60;$i++){ Start-Sleep 1; $l=Get-ChildItem "$root\Logs" | Where-Object { $_.CreationTime -gt $t0 } | Sort-Object LastWriteTime | Select-Object -Last 1; if(-not $l){ continue }
  $m=Select-String -Path $l.FullName -Pattern "\[engagetouch\] hook TickInput" ; if($m){ Select-String -Path $l.FullName -Pattern "engagetouch" | ForEach-Object { $_.Line }; exit 0 } }
"TIMEOUT"
