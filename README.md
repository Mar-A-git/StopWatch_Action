# StopWatch_Action

M5Stack StopWatch を手に着けて、アクションで遊ぶためのファームウェア集。

| フォルダ | 内容 |
|---|---|
| [stackchan_watch](stackchan_watch/) | **スタックチャン時計＋かめはめ波**（新しい版）。起動メニューで、短針と同じに回るスタックチャン時計・かめはめ波（左・右）・自分のモード（空き）を切り替える |
| [kamehameha](kamehameha/) | **かめはめ波**（最初の版。note の記事の手順はこちら）。2台を左右の手のひらに着けて、向かい合わせて溜め、両手を突き出して撃つ（光・声・振動つき） |
| [rssi_check](rssi_check/) | 2台の ESP-NOW の受信レベル（RSSI）を画面に出す実験機。かめはめ波の判定の値は、これで測った |

> ⚠ **ドラゴンボールの「かめはめ波」をイメージした、非公式のファンメイドです。**
> 原作・権利者とは関係ありません。MIT ライセンスはこのリポジトリのプログラムだけに適用されます。
> 「かめはめ波」などの名前や表現を使う時は、各自で権利に気をつけてください。
> スタックチャン時計も、スタックチャンの非公式のファンメイドです。

解説記事（note）
- かめはめ波（kamehameha・rssi_check）: [修行したらアレが出せちゃった件の報告](https://note.com/mar_69/n/n1c664bc42495)
- スタックチャン時計（stackchan_watch）: [スタックチャン時計を追加してみた](https://note.com/mar_69/n/nab9e43a1143e)

## 必要なもの

- M5Stack StopWatch（C152）… かめはめ波は2台。このファームウェアは StopWatch でだけ動作を確認しています
- データ通信ができる USB-C ケーブル（付属のケーブルは充電専用のことがあります）
- VS Code ＋ PlatformIO

ライブラリ（M5Unified・M5GFX）は PlatformIO が自動で取ってきます。

### 腕時計にするケース

StopWatch を腕時計として着けるための取り付けベースとプロテクターの 3D データは、Kawacci さんが公開しています。

- [kawacci/m5-stopwatch-watch-case](https://github.com/kawacci/m5-stopwatch-watch-case)

## はじめかた

**スタックチャン時計とかめはめ波を両方使う（新しい版）**

1. [stackchan_watch](stackchan_watch/) を書き込む（プログラムと声の2つ。かめはめ波は2台とも同じものを書き込む）
2. メニューでモードを選ぶ。A と B を同時に 1.5秒押すと、いつでもメニューに戻れる

**かめはめ波だけ（最初の版）**

1. まず [rssi_check](rssi_check/) を2台に書き込んで、手のひらを向かい合わせた時と開いた時の受信レベルを見てみる
2. [kamehameha](kamehameha/) を2台に書き込む（プログラムと声のファイルの2つ）
3. 手のひらを向かい合わせて溜め、④の「め」で両手を突き出す

うまく判定されない時は、`kamehameha/src/config.h`（新しい版は `stackchan_watch/src/kame/config.h`）のしきい値を、自分の手や着け方に合わせて調整してください。

## この作品について（自己責任でお願いします）

- **AI（Claude）と一緒に作りました。** 細かいところまでは精査しきれていません
- 動作は保証しません。使う時は自己責任でお願いします（LICENSE の「無保証」のとおりです）
- 振動・画面の明るさ（発射の時は最大）・無線の送信（21dBm）を使うので、電池の減りは早めです
- 別のボードに移植する時は、**技適のある機器**で使ってください

不具合の報告・改善案・改造の Issue や Pull Request は大歓迎です。みんなでよくしていきましょう。

## クレジット・ライセンス

- プログラム: [MIT License](LICENSE)
- スタックチャンは、ししかわさん（Shinya Ishikawa）が生み出したロボットです。
  スタックチャン時計の顔は [M5Stack-Avatar](https://github.com/stack-chan/m5stack-avatar)（Copyright (c) 2018 Shinya Ishikawa, MIT License）をもとにしています（全文は LICENSE の Third-party notice）
- 声: **VOICEVOX:ずんだもん**（`kamehameha/data/voice/`・`stackchan_watch/data/`）
  - この WAV には MIT ライセンスは適用されません
  - 使う・改造して配る時は、クレジット「VOICEVOX:ずんだもん」を付け、
    [VOICEVOX の利用規約](https://voicevox.hiroshiba.jp/term/) と
    [ずんだもんの音源利用規約](https://zunko.jp/con_ongen_kiyaku.html)（禁止事項を含む）に従ってください
- 腕時計にするケース: [Kawacci さんの m5-stopwatch-watch-case](https://github.com/kawacci/m5-stopwatch-watch-case)（データはこのリポジトリには含みません）
