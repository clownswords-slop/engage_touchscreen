# restart.ps1 [release|debug] [npdm path]: stop Ryujinx, install the debug (or release) build, boot Engage, wait for the
# mod's init log line. No main.npdm is installed unless one is given (e.g. build\main.npdm from tools/mknpdm.py for a
# standalone setup, or Cobalt-master\packaged\exefs\main.npdm); an npdm already in the mod folder is left alone.
param([string]$kind="debug", [string]$npdm="")
$root="E:\emulators\Ryujinx"; $mod="$env:APPDATA\Ryujinx\mods\contents\0100a6301214e000\EngageTouchMod\exefs"
Set-Content -Path "$env:APPDATA\Ryujinx\sdcard\engagetouch\cmd.txt" -Value "2`ninfo" -Encoding ascii
Get-Process Ryujinx -ErrorAction SilentlyContinue | Stop-Process; Start-Sleep 3
$src= if($kind -eq "release"){ "subsdk8" } else { "subsdk8_dbg" }
Remove-Item "$mod\subsdk9" -Force -ErrorAction SilentlyContinue   # the slot the mod used before 2026-11-12
Copy-Item "$root\claude_engage_touchmod\build\$src" "$mod\subsdk8" -Force
if($npdm){ Copy-Item $npdm "$mod\main.npdm" -Force }
$t0=Get-Date
Start-Process -FilePath "$root\Ryujinx.exe" -ArgumentList "`"$root\Fire Emblem Engage [0100A6301214E000][v0].nsp`"" -WorkingDirectory $root
for($i=0;$i -lt 60;$i++){ Start-Sleep 1; $l=Get-ChildItem "$root\Logs" | Where-Object { $_.CreationTime -gt $t0 } | Sort-Object LastWriteTime | Select-Object -Last 1; if(-not $l){ continue }
  $m=Select-String -Path $l.FullName -Pattern "\[engagetouch\] hook TickInput" ; if($m){ Select-String -Path $l.FullName -Pattern "engagetouch" | ForEach-Object { $_.Line }; exit 0 } }
"TIMEOUT"
