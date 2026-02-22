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

---
以下これとは別で環境構築した場合について

VisualStudio->プロジェクトのプロパティ->[構成プロパティ]
->[C/C++]->[全般]の中の[追加のインクルードディレクトリ]より，
Boost，およびlibqrencodeのディレクトリを指定します．

**libqrencodeはdllを必要とする**ため，同様にプロジェクトのプロパティ->
ビルドイベント->ビルド前のイベントに，dllをexeが生成されるディレクトリに
コピーするように宣言します．ここで，**DebugとReleaseでは動的リンクするべきファイルが異なる**点に注意します．これを行わない場合，ビルド時にそのディレクトリ内の
ファイルがすべて削除されるため，毎回dllを配置しなければなりません．

例:
```sh
copy /Y "C:\vcpkg\installed\x64-windows\bin\qrencode.dll" "$(OutDir)"
```