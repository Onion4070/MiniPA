# MiniPA

## 環境構築
- Boost  
https://www.boost.org/releases/latest/  
から最新版をダウンロードし，VisualStudio->プロジェクトのプロパティ->[構成プロパティ]
->[C/C++]->[全般]の中の[追加のインクルードディレクトリ]からBoostのディレクトリを指定します．

- libqrencode  
https://fukuchi.org/works/qrencode/  
vcpkgをC直下にインストールし，libqrencodeをインストールします．
```sh
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
bootstrap-vcpkg.bat
vcpkg integrate install
vcpkg install libqrencode
```

この通りにインストールできればvcxprojの設定を変えずに環境構築が完了します．

- libopus  
librencodeと同様にして，vcpkg経由でインストールします．
```sh
vcpkg install opus
```

この通りにインストールできればvcxprojの設定を変えずに環境構築が完了します．

---
以下これとは別で環境構築した場合について

VisualStudio->プロジェクトのプロパティ->[構成プロパティ]
->[C/C++]->[全般]の中の[追加のインクルードディレクトリ]より，
Boost，libqrencode，およびlibopusのインクルードディレクトリを指定します．

リンカー->追加のライブラリディレクトリにlibqrencode・libopusのライブラリディレクトリを指定し，
リンカー->入力から，ライブラリの実体ファイルの名前を指定します．(`qrencode.lib`, `opus.lib`)

**libqrencodeはdllを必要とする**ため，同様にプロジェクトのプロパティ->
ビルドイベント->ビルド前のイベントに，dllをexeが生成されるディレクトリに
コピーするように宣言します．これを行わない場合，ビルド時にそのディレクトリ内の
ファイルがすべて削除されるため，毎回dllを配置しなければなりません．

例:
```sh
copy /Y "C:\vcpkg\installed\x64-windows\bin\qrencode.dll" "$(OutDir)"
```

**libopusも同様にdllが必要**です．ビルド前のイベントに以下を追加してください．

例:
```sh
copy /Y "C:\vcpkg\installed\x64-windows\bin\opus.dll" "$(OutDir)"
```