# toraIME をアンインストールする (管理者として実行してください)
#Requires -RunAsAdministrator
$ErrorActionPreference = 'Stop'
$root = Join-Path $env:ProgramFiles 'toraIME'
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

# 登録を解除する (インストールされているすべてのバージョン)
$regsvr = @{
  'x64' = Join-Path $env:WINDIR 'System32\regsvr32.exe'
  'x86' = Join-Path $env:WINDIR 'SysWOW64\regsvr32.exe'
}
if (Test-Path $root) {
  Get-ChildItem $root -Recurse -Filter 'toraime.dll' | ForEach-Object {
    $exe = $regsvr[$_.Directory.Name]
    if ($exe) {
      Start-Process $exe -ArgumentList '/u', '/s', "`"$($_.FullName)`"" -Wait -WindowStyle Hidden | Out-Null
    }
  }
}
Write-Host 'IME の登録を解除しました。'

$shortcut = Join-Path ([Environment]::GetFolderPath('CommonPrograms')) 'toraIME の設定.lnk'
if (Test-Path $shortcut) { Remove-Item $shortcut -Force }

try {
  Remove-Item $root -Recurse -Force -ErrorAction Stop
  Write-Host "$root を削除しました。"
} catch {
  Write-Host "使用中のファイルがあるため $root を削除できませんでした。再起動後に手動で削除してください。"
}
Write-Host '設定 (HKCU\Software\toraIME) と学習履歴 (%APPDATA%\toraIME) は残しています。不要なら削除してください。'
