"""
スタックチャン時計の声（ずんだもん）を VOICEVOX で作る

・VOICEVOX（PC上で動く無料の音声合成ソフト）を起動しておく。
  http://127.0.0.1:50021 に問い合わせるだけで、インターネットや有料サービスにはつながない
・「いまは」「ごご」「さんじ」「じゅうごふん」「なのだ」のように部品ごとに作り、
  StopWatch でつなげて喋らせる
・普通の長さの言葉なので、母音を伸ばしすぎてジジジとなる問題は起きない
・出力: data/stackchan/voice/*.wav（24kHz / 16bit / モノラル）

使い方（PlatformIO に入っている Python。追加のインストールは不要）
  %USERPROFILE%\\.platformio\\penv\\Scripts\\python.exe make_stackchan_voice.py
  声の種類を変える: --style 7（ツンツン）など。--list で一覧

クレジット: VOICEVOX:ずんだもん
"""

import argparse
import array
import io
import json
import os
import sys
import urllib.error
import urllib.parse
import urllib.request
import wave

ENGINE = "http://127.0.0.1:50021"
HERE = os.path.dirname(os.path.abspath(__file__))
OUT_DIR = os.path.normpath(os.path.join(HERE, "..", "..", "data", "stackchan", "voice"))
RATE = 24000
PEAK = 0.5  # 一番大きい所（上限に対する割合）。StopWatch は 0.5 を超えると割れやすい

HOURS = ["いちじ", "にじ", "さんじ", "よじ", "ごじ", "ろくじ",
         "しちじ", "はちじ", "くじ", "じゅうじ", "じゅういちじ", "じゅうにじ"]
TENS = ["", "じゅう", "にじゅう", "さんじゅう", "よんじゅう", "ごじゅう"]
ONES = ["", "いち", "に", "さん", "よん", "ご", "ろく", "なな", "はち", "きゅう"]
ONES_FUN = ["", "いっぷん", "にふん", "さんぷん", "よんぷん", "ごふん",
            "ろっぷん", "ななふん", "はっぷん", "きゅうふん"]
TENS_FUN = ["", "じゅっぷん", "にじゅっぷん", "さんじゅっぷん", "よんじゅっぷん", "ごじゅっぷん"]


def minute_reading(m):
    if m == 0:
        return "ちょうど"
    t, o = divmod(m, 10)
    if o == 0:
        return TENS_FUN[t]
    return TENS[t] + ONES_FUN[o]


def phrases():
    items = [("intro.wav", "いまは"), ("outro.wav", "なのだ"),
             ("am.wav", "ごぜん"), ("pm.wav", "ごご")]
    for h in range(1, 13):
        items.append((f"h{h:02d}.wav", HOURS[h - 1]))
    for m in range(60):
        items.append((f"m{m:02d}.wav", minute_reading(m)))
    return items


def request(path, params=None, body=None, method="POST"):
    url = ENGINE + path
    if params:
        url += "?" + urllib.parse.urlencode(params)
    data = json.dumps(body).encode("utf-8") if body is not None else None
    headers = {"Content-Type": "application/json"} if body is not None else {}
    req = urllib.request.Request(url, data=data, headers=headers, method=method)
    with urllib.request.urlopen(req, timeout=60) as res:
        return res.read()


def synth(text, speaker, speed):
    q = json.loads(request("/audio_query", {"text": text, "speaker": speaker}))
    q["speedScale"] = speed
    q["prePhonemeLength"] = 0.03
    q["postPhonemeLength"] = 0.05
    q["outputSamplingRate"] = RATE
    q["outputStereo"] = False
    wav = request("/synthesis", {"speaker": speaker}, body=q)
    with wave.open(io.BytesIO(wav)) as r:
        a = array.array("h", r.readframes(r.getnframes()))
    return a


def write(path, a):
    top = max((abs(v) for v in a), default=0) or 1
    k = PEAK * 32767 / top
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes(array.array("h", (int(v * k) for v in a)).tobytes())
    return len(a) / RATE


def main():
    ap = argparse.ArgumentParser(description="スタックチャン時計の声を VOICEVOX で作る")
    ap.add_argument("--list", action="store_true", help="ずんだもんの声の種類（ID）を表示する")
    ap.add_argument("--style", type=int, default=3, help="声の ID（3 = ずんだもん ノーマル）")
    ap.add_argument("--speed", type=float, default=1.0, help="話す速さ（1.0 が普通）")
    args = ap.parse_args()

    try:
        urllib.request.urlopen(ENGINE + "/version", timeout=3).read()
    except (urllib.error.URLError, OSError):
        sys.exit("VOICEVOX が見つかりません。VOICEVOX を起動してから、もう一度実行してください。")

    if args.list:
        for sp in json.loads(request("/speakers", method="GET")):
            if "ずんだもん" in sp["name"]:
                for st in sp["styles"]:
                    print(f"{st['id']:4d}  {sp['name']}（{st['name']}）")
        return

    os.makedirs(OUT_DIR, exist_ok=True)
    items = phrases()
    total = 0.0
    for i, (name, text) in enumerate(items, 1):
        total += write(os.path.join(OUT_DIR, name), synth(text, args.style, args.speed))
        print(f"[{i:2d}/{len(items)}] {name}  {text}")
    print(f"完了: {len(items)} ファイル（合計 {total:.1f} 秒）→ {OUT_DIR}")
    print("クレジット: VOICEVOX:ずんだもん")


if __name__ == "__main__":
    main()
