# toraIME をアンインストールする (管理者として実行してください)
#Requires -RunAsAdministrator
$ErrorActionPreference = 'Stop'
$dest = Join-Path $env:ProgramFiles 'toraIME'
$clsid = '{6BA3DA2F-6C06-4E10-AC8D-1350C0344633}'
$profileGuid = '{FF09BE99-990B-4DFC-8946-FD1A77950A82}'

# キーボード一覧から外す
$tip = "0411:$clsid$profileGuid"
$list = Get-WinUserLanguageList
$changed = $false
foreach ($lang in $list) {
  if ($lang.InputMethodTips -contains $tip) {
    $lang.InputMethodTips.Remove($tip) | Out-Null
    $changed = $true
  }
}
if ($changed) { Set-WinUserLanguageList $list -Force }

$targets = @(
  @{ Dir = 'x64'; RegSvr = Join-Path $env:WINDIR 'System32\regsvr32.exe' },
  @{ Dir = 'x86'; RegSvr = Join-Path $env:WINDIR 'SysWOW64\regsvr32.exe' }
)
foreach ($t in $targets) {
  $dll = Join-Path $dest "$($t.Dir)\toraime.dll"
  if (Test-Path $dll) {
    Start-Process $t.RegSvr -ArgumentList '/u', '/s', "`"$dll`"" -Wait -WindowStyle Hidden | Out-Null
  }
}
Write-Host 'IME の登録を解除しました。'

try {
  Remove-Item $dest -Recurse -Force
  Write-Host "$dest を削除しました。"
} catch {
  Write-Host "使用中のファイルがあるため $dest を削除できませんでした。再起動後に手動で削除してください。"
}
Write-Host '設定 (HKCU\Software\toraIME) と学習履歴 (%APPDATA%\toraIME) は残しています。不要なら削除してください。'
