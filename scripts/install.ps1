# toraIME をインストールする (管理者として実行してください)
#   install.cmd をダブルクリックするか、管理者の PowerShell で .\install.ps1 を実行します。
#   -NoLanguageList を付けると、日本語のキーボード一覧への追加を行いません。
#
# 起動中のアプリは古い IME (DLL と辞書) を読み込んだままなので、上書きはできません。
# そのため毎回新しいフォルダー (C:\Program Files\toraIME\<日時>) に入れて登録し直します。
# 古いフォルダーは、使われなくなっていれば次のインストール時に削除します。
#Requires -RunAsAdministrator
param([switch]$NoLanguageList)

$ErrorActionPreference = 'Stop'
$src = $PSScriptRoot
$root = Join-Path $env:ProgramFiles 'toraIME'
$clsid = '{6BA3DA2F-6C06-4E10-AC8D-1350C0344633}'
$profileGuid = '{FF09BE99-990B-4DFC-8946-FD1A77950A82}'
$targets = @(
  @{ Dir = 'x64'; RegSvr = Join-Path $env:WINDIR 'System32\regsvr32.exe' },
  @{ Dir = 'x86'; RegSvr = Join-Path $env:WINDIR 'SysWOW64\regsvr32.exe' }
)

if ($env:PROCESSOR_ARCHITECTURE -ne 'AMD64') {
  throw 'toraIME は現在 x64 版の Windows だけに対応しています。'
}

function Invoke-RegSvr($regsvr, [string[]]$arguments) {
  $p = Start-Process $regsvr -ArgumentList $arguments -Wait -PassThru -WindowStyle Hidden
  return $p.ExitCode
}

# 新しいフォルダーにコピーする
$version = Get-Date -Format 'yyyyMMddHHmmss'
$dest = Join-Path $root $version
Write-Host "インストール先: $dest"
foreach ($t in $targets) {
  New-Item -ItemType Directory -Force (Join-Path $dest $t.Dir) | Out-Null
  Copy-Item (Join-Path $src "$($t.Dir)\toraime.dll") (Join-Path $dest "$($t.Dir)\toraime.dll") -Force
}
foreach ($f in 'toraime.dic', 'english_words.txt', 'english_large.txt', 'toraime_settings.exe',
               'MOZC_DICTIONARY_README.txt', 'ENGLISH_WORDS_README.txt', 'README.md') {
  if (Test-Path (Join-Path $src $f)) { Copy-Item (Join-Path $src $f) (Join-Path $dest $f) -Force }
}
foreach ($f in 'uninstall.ps1', 'uninstall.cmd') {
  if (Test-Path (Join-Path $src $f)) { Copy-Item (Join-Path $src $f) (Join-Path $root $f) -Force }
}

# 新しい DLL を登録する (以前の登録は上書きされる)
foreach ($t in $targets) {
  $dll = Join-Path $dest "$($t.Dir)\toraime.dll"
  $code = Invoke-RegSvr $t.RegSvr @('/s', "`"$dll`"")
  if ($code -ne 0) { throw "$dll の登録に失敗しました (regsvr32: $code)" }
}
Write-Host 'IME を登録しました。'

# スタートメニューに設定アプリのショートカットを作る
# (WScript.Shell は日本語のファイル名を扱えない環境があるので、英字の名前で作ってから名前を変える)
$programs = [Environment]::GetFolderPath('CommonPrograms')
$shortcut = Join-Path $programs 'toraIME の設定.lnk'
$temp = Join-Path $programs 'toraIME-settings.lnk'
$shell = New-Object -ComObject WScript.Shell
$link = $shell.CreateShortcut($temp)
$link.TargetPath = Join-Path $dest 'toraime_settings.exe'
$link.IconLocation = (Join-Path $dest 'toraime_settings.exe') + ',0'
$link.Save()
Move-Item -LiteralPath $temp -Destination $shortcut -Force
Write-Host 'スタートメニューに「toraIME の設定」を追加しました。'

# 古いバージョンを片付ける (使用中のものは残す)
$kept = @()
foreach ($item in Get-ChildItem $root) {
  if ($item.FullName -eq $dest -or $item.Name -like 'uninstall.*') { continue }
  try {
    Remove-Item $item.FullName -Recurse -Force -ErrorAction Stop
  } catch {
    $kept += $item.Name
  }
}
if ($kept.Count -gt 0) {
  Write-Host "使用中のため残した古いファイル: $($kept -join ', ') (次回のインストール時か、再起動後に削除できます)"
}

if (-not $NoLanguageList) {
  $tip = "0411:$clsid$profileGuid"
  $list = Get-WinUserLanguageList
  $ja = $list | Where-Object { $_.LanguageTag -eq 'ja' }
  if (-not $ja) {
    $list.Add('ja')
    $ja = $list | Where-Object { $_.LanguageTag -eq 'ja' }
  }
  if ($ja.InputMethodTips -notcontains $tip) {
    $ja.InputMethodTips.Add($tip)
    Set-WinUserLanguageList $list -Force
  }
  Write-Host '日本語のキーボードに toraIME を追加しました。Win + Space で切り替えられます。'
}
Write-Host '完了しました。すでに起動しているアプリでは、アプリを再起動すると新しいバージョンになります。'
