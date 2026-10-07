# toraIME をインストールする (管理者として実行してください)
#   install.cmd をダブルクリックするか、管理者の PowerShell で .\install.ps1 を実行します。
#   -NoLanguageList を付けると、日本語のキーボード一覧への追加を行いません。
#Requires -RunAsAdministrator
param([switch]$NoLanguageList)

$ErrorActionPreference = 'Stop'
$src = $PSScriptRoot
$dest = Join-Path $env:ProgramFiles 'toraIME'
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

# 使用中の DLL は上書きできないので、名前を変えてから置き換える
function Copy-Replacing($from, $to) {
  if (Test-Path $to) {
    try {
      Remove-Item $to -Force
    } catch {
      $old = "$to.$([DateTime]::Now.ToString('yyyyMMddHHmmss')).old"
      Rename-Item $to $old -Force
      Write-Host "使用中のファイルを $old に退避しました (再起動後に削除できます)"
    }
  }
  Copy-Item $from $to -Force
}

Write-Host "インストール先: $dest"
foreach ($t in $targets) {
  New-Item -ItemType Directory -Force (Join-Path $dest $t.Dir) | Out-Null
  Copy-Replacing (Join-Path $src "$($t.Dir)\toraime.dll") (Join-Path $dest "$($t.Dir)\toraime.dll")
}
foreach ($f in 'toraime.dic', 'english_words.txt', 'MOZC_DICTIONARY_README.txt', 'README.md', 'uninstall.ps1', 'uninstall.cmd') {
  if (Test-Path (Join-Path $src $f)) { Copy-Replacing (Join-Path $src $f) (Join-Path $dest $f) }
}

foreach ($t in $targets) {
  $dll = Join-Path $dest "$($t.Dir)\toraime.dll"
  $code = Invoke-RegSvr $t.RegSvr @('/s', "`"$dll`"")
  if ($code -ne 0) { throw "$dll の登録に失敗しました (regsvr32: $code)" }
}
Write-Host 'IME を登録しました。'

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
Write-Host '完了しました。すでに起動しているアプリでは、アプリを再起動すると使えるようになります。'
