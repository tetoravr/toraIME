# toraIME

Windows 11 向けの日本語 IME です。

- **自動で変換します**: 打つそばから漢字かな交じりに変換して表示します (ライブ変換)。そのまま Enter で確定できます。打った読みは未確定文字列のすぐ下に小さく表示されるので、どこまで入力したかも分かります。
- **英字は変換しません**: `windows`、`test`、`iPhone`、`PC` のような英単語は、ローマ字にせず英字のまま入力されます。
- **全角とかな入力を無効にできます**: 全角英数字・全角記号・全角スペースを一切出さない設定 (既定) と、うっかり切り替わりがちな「かな入力」を無効にする設定 (既定) があります。

```
watashihaenglishgasuki   → 私はenglishが好き
Windowsnosettei          → Windowsの設定
iPhonewokatta            → iPhoneを買った
PCwokidou                → PCを起動
githubnirepositorywotsukuru → githubにrepositoryを作る
kaigiha3jikaradesu       → 会議は3時からです
```

## インストール

1. [Actions](../../actions) の最新のビルドから `toraIME` をダウンロードして展開します。
2. `install.cmd` をダブルクリックします (管理者の確認が出ます)。
   - `C:\Program Files\toraIME\<日時>` にコピーし、64bit/32bit の両方のアプリで使えるように登録します。
   - 新しいバージョンに入れ替えるときも、アプリを閉じずにそのまま実行できます (起動中のアプリは再起動すると新しいバージョンになります)。
   - 日本語のキーボード一覧に toraIME を追加します。
3. **Win + Space** で toraIME に切り替えます。起動中のアプリは再起動すると使えるようになります。
4. 設定はスタートメニューの「**toraIME の設定**」か、タスクバーの「あ」を右クリックして変えられます。

アンインストールは `C:\Program Files\toraIME\uninstall.cmd` を実行します。

> 対応しているのは x64 版の Windows 10/11 です。ARM64 版 Windows にはまだ対応していません。

## 使い方

### 入力中 (下線が点線)

未確定文字列の下に、打った読み (例: `わたしはがっこうにいk`) が表示されます。

| キー | 動作 |
| --- | --- |
| 文字キー | 入力して、自動で変換した結果を表示する |
| Enter | 表示されている内容で確定する |
| Space / ↓ / → | 変換中にする (文節ごとに候補を選べる) |
| ← | 変換中にして、最後の文節を選ぶ |
| Shift + Space | 空白を入れる (英文を打つとき) |
| BackSpace | 1 文字消す |
| Esc | 1 回目: 変換前のひらがなに戻す / 2 回目: 取り消す |
| F6 / F7 / F8 / F9 / F10 | ひらがな / カタカナ / 半角カナ ※ / 全角英数 ※ / 半角英数 (F10 を続けて押すと小文字・大文字・先頭だけ大文字) |

※ 半角カナと全角英数は、設定で有効にしたときだけ使えます。

### 変換中 (注目している文節が太い下線、候補ウィンドウが出る)

| キー | 動作 |
| --- | --- |
| Space / ↓ / Tab | 次の候補 |
| Shift + Space / ↑ | 前の候補 |
| 1〜9 / クリック | 候補を選ぶ |
| PageDown / PageUp | 次 / 前のページ |
| ← / → | 文節を移動する |
| Shift + ← / → | 文節の長さを変える |
| Enter | 確定する (選んだ候補を学習します) |
| Esc / BackSpace | 入力中に戻る |
| 文字キー | 確定して、続けて入力する |

### IME のオン/オフ

| キー | 動作 |
| --- | --- |
| 半角/全角、Alt + ` | オン/オフを切り替える |
| 変換 / 無変換 | オン / オフ (設定で無効にできます。入力中の 変換 は Space、無変換 はカタカナ) |
| Alt + カタカナひらがな | かな入力とローマ字入力の切り替え (かな入力を有効にしたときだけ。無効なら何も起きません) |

タスクバーの「あ」「A」をクリックするとオン/オフ、右クリックすると設定メニューが出ます。

### 記号

| 入力 | 結果 |
| --- | --- |
| `[` `]` | 「 」 |
| `,` `.` `/` `-` | 、 。 ・ ー |
| `zh` `zj` `zk` `zl` | ← ↓ ↑ → |
| `z.` `z,` `z/` `z-` `z[` `z]` | … ‥ ・ 〜 『 』 |

記号だけの文節を Space で変換すると、`()` → 「」『』【】〈〉《》…、`*` → ※★☆×… のような記号の候補が出ます (全角英数字が無効なときは （） ＊ などの全角記号は出ません)。

## 英字の判定

ひらがなモードのまま英単語を打っても、次の場合は英字のまま入力されます。

1. **英単語リストにある語** (`data/english_words.txt`): `game`、`test`、`windows` など。ただし `made` (まで) や `site` (して) のように日本語として自然なものは日本語が優先されます。
2. **ローマ字にならない綴りを含む語**: `xyzzy`、`nft` など。
3. **大文字で始めた語**: `Claude`、`Tokyo`、`PC` など。続けて日本語を打っても、人名・地名を含む英単語辞書で語の切れ目を判断します (`Claudenikiku` → Claudeに聞く、`PCwokidou` → PCを起動)。辞書に無い語は英字が続くところまで英字になります。
4. **一度英字を選んだ語**: 変換中に F10 や候補で英字を選ぶと学習して、次から英字になります。

自分で単語を足したいときは、設定メニューの「英単語リストを開く」(`%APPDATA%\toraIME\user_english.txt`) に 1 行 1 語で書きます。

## 設定

スタートメニューの「toraIME の設定」か、タスクバーの入力モード表示 (「あ」) を右クリックしたメニューで変えられます。設定は `HKEY_CURRENT_USER\Software\toraIME` に保存され、起動中のアプリにも次に入力欄を選んだときに反映されます。

| メニュー | 値の名前 | 既定 |
| --- | --- | --- |
| 入力中の表示: 変換しない (Space で変換) / すべて自動で変換する / 入力中の文節はひらがなのまま | `LiveConversion` (0/1/2) | すべて自動で変換する |
| 入力した読みを下に表示する | `ReadingHint` | オン |
| 英単語は英字のまま入力する | `EnglishDetection` | オン |
| 変換を学習する | `Learning` | オン |
| 変換キーでオン / 無変換キーでオフ | `ConvertKeysOnOff` | オン |
| CapsLock を無効にする | `DisableCapsLock` | オン |
| 全角英数字: 使わない / 候補と F9 だけで使う / 入力した英数字を全角にする | `FullWidth` (0/1/2) | 使わない |
| かな入力を使えるようにする | `KanaInput` | オフ |
| 半角カタカナを使う | `HalfWidthKana` | オフ |
| 句読点: 、。 / ，． / ，。 / 、． | `Punctuation` (0〜3) | 、。 |

「入力中の文節はひらがなのまま」では、いま打っている文節だけ読みのまま表示し、次の文節を打ち始めると前の文節が漢字になります。Enter では表示されているとおりに確定するので、最後の文節を漢字にしたいときは Space で変換してから確定します。

「CapsLock を無効にする」では、toraIME を使っている間に CapsLock (JIS キーボードでは Shift+英数) がオンになると、すぐにオフに戻します。オンのままになっていても、入力する英字は Shift キーだけで大文字・小文字が決まります。

「全角英数字: 使わない」では、全角英数モード・F9・全角の候補・全角スペースがすべて出なくなり、`!` `?` などの記号も半角になります (「、」「。」「ー」「「」」などの日本語の記号はそのままです)。

### ユーザー辞書

設定メニューの「ユーザー辞書を開く」で `%APPDATA%\toraIME\user_dict.txt` がメモ帳で開きます。

```
# 読み<TAB>表記<TAB>コスト(省略可、小さいほど優先)
とらいめ	toraIME
```

学習履歴は `%APPDATA%\toraIME\history.tsv` です。メニューの「学習履歴を消去」で消せます。

## ソースからビルドする

必要なもの: Visual Studio 2022 (C++ によるデスクトップ開発)、CMake 3.20 以上、Python 3

```powershell
# 辞書 (Mozc のオープンソース辞書をダウンロードして変換します)
python tools/fetch_mozc.py build/mozc
python tools/build_dict.py build/mozc build/toraime.dic
python tools/build_english.py build   # 大文字で始めた語の判定に使う英単語リスト

# 64bit 版と 32bit 版の DLL
cmake -S . -B build/x64 -A x64
cmake --build build/x64 --config Release
cmake -S . -B build/x86 -A Win32
cmake --build build/x86 --config Release

# テスト
ctest --test-dir build/x64 -C Release --output-on-failure
```

開発中は、`toraime.dic`・`english_large.txt`・`data/english_words.txt` を DLL と同じフォルダー (またはその親フォルダー) に置き、管理者のコマンドプロンプトで `regsvr32 toraime.dll` すると登録できます。

変換エンジン (`src/engine`) は OS に依存しないので、Linux や macOS でもビルドしてテストできます。

```sh
cmake -S . -B build -DTORA_DIC=$PWD/build/toraime.dic
cmake --build build && ctest --test-dir build --output-on-failure
```

## しくみ

```
src/engine/   変換エンジン (OS 非依存)
  romaji      ローマ字 → ひらがな
  composer    打鍵を入力単位 (ローマ字 1 音節・英字 1 文字など) にまとめる
  input       入力単位を「打鍵した文字の位置」で扱う (英単語の途中で区切ったら残りをローマ字として読み直す)
  dictionary  システム辞書 (メモリマップして全プロセスで共有)
  converter   ラティスを作り、単語コスト + 連接コストが最小の経路を Viterbi で探す
              英単語・英字列・大文字で始まる語もノードとして日本語と競わせる
  session     入力中 / 変換中 の状態機械とキー操作
src/tsf/      Windows の Text Services Framework (TSF) テキストサービス
src/settings_app/  設定アプリ (toraime_settings.exe)
tools/        辞書の生成 (Mozc OSS 辞書 → toraime.dic)、アイコンの生成
tests/        エンジンのテスト、TSF のスモークテスト
```

## 制限事項

- 実機の Windows での動作確認はまだ十分ではありません (CI ではビルド・テスト・COM 登録・TSF の有効化までを確認しています)。
- 予測変換、再変換、変換の取り消し (Undo) はありません。
- 全画面のゲームなどで使われる UI-less モード (ITfUIElement) には対応していません。
- 辞書は Mozc のオープンソース版 (IPAdic ベース) で、Google 日本語入力の辞書より語彙が少なめです。

## ライセンス

英単語リスト `english_large.txt` は [SCOWL](http://wordlist.aspell.net/) 由来の Hunspell 英語辞書 (LibreOffice の dictionaries リポジトリ) から生成しています。ライセンスは配布物の `ENGLISH_WORDS_README.txt` を参照してください。アイコンは `assets/icon.png` です。

辞書データは [Mozc](https://github.com/google/mozc) のオープンソース辞書 (`src/data/dictionary_oss`) から生成しています。辞書のライセンス (IPAdic・沖縄辞書など) は配布物の `MOZC_DICTIONARY_README.txt` を参照してください。
