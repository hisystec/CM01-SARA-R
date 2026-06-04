# CM01-SARA-R

ESP32 Arduino IDE 向けの CM01-SARA-R 通信モジュール制御ライブラリです。u-blox SARA-R510S-61B を搭載した CM01-SARA-R と UART で通信し、AT コマンドの送受信、非同期レスポンスの受信、HTTP/HTTPS 通信サンプルを扱えます。

## 特長

- ESP32 の `HardwareSerial` を使った SARA-R モデム制御
- 電源制御ピン、PWR_ON ピン、UART ピン、RTS/CTS の設定
- AT コマンド送信と応答待ち
- `+UUPSDA:` や `+UUHTTPCR:` などの非同期イベント受信
- プロンプト付きデータ送信への対応
- HTTP GET / HTTP POST / CA 証明書管理のサンプル付き
- デバッグログ出力の有効化、無効化

## 対象環境

- ESP32
- Arduino IDE または arduino-cli
- CM01-SARA-R 通信モジュール
- SORACOM など LTE-M 回線で利用できる SIM

## 推奨配線

examples では次のピンを前提にしています。

| 信号 | ESP32 GPIO | 用途 |
| --- | ---: | --- |
| EN | 5 | CM01-SARA-R 電源 enable |
| PWR_ON | 4 | モデム電源 ON 制御 |
| RXD | 16 | ESP32 受信 |
| TXD | 17 | ESP32 送信 |
| RTS | 18 | ハードウェアフロー制御 |
| CTS | 19 | ハードウェアフロー制御 |

UART 通信の安定性のため、RTS/CTS を接続してハードウェアフロー制御を有効にすることを推奨します。

## インストール

Arduino IDE のライブラリフォルダへ、このリポジトリを `CM01-SARA-R` として配置してください。

例:

```text
Arduino/libraries/CM01-SARA-R/
```

スケッチからは次のように読み込みます。

```cpp
#include <CM01-SARA-R.h>
```

## 基本的な使い方

```cpp
#include <Arduino.h>
#include <CM01-SARA-R.h>

ModemHandler modem(Serial2);

void setup() {
  Serial.begin(115200);

  modem.setPins(5, 4, 16, 17, 18, 19, true);
  modem.setAsyncResponsePrefixes({"+UUPSDA:", "+UUHTTPCR:"});
  modem.setResponseEndCriteria({"OK", "ERROR", "+CME ERROR:*", "+CMS ERROR:*"});
  modem.begin();

  std::vector<String> responses;
  if (modem.sendATCommandWithResponse("AT", &responses, 5000)) {
    Serial.println("Modem is ready.");
  }
}

void loop() {
}
```

通常は examples のスケッチを出発点にしてください。APN、接続先、証明書の有無などを用途に合わせて変更します。

## Examples

| Example | 内容 |
| --- | --- |
| `AT_Debug` | シリアルモニタから任意の AT コマンドを送信するデバッグ用スケッチ |
| `http_get` | SORACOM APN で接続し、`https://hi-corp.net/` へ HTTP GET するサンプル |
| `http_post` | JSON をモデム内ファイルへ保存し、`https://hi-corp.net/` へ HTTP POST するサンプル |
| `resister_CA` | CA 証明書をモデムへ登録するサンプル |
| `management_CA` | 登録済み CA 証明書の確認、削除を行うサンプル |

### `http_get`

`examples/http_get/http_get.ino` は次の流れで HTTPS GET を行います。

1. `soracom.io` APN を設定
2. PSD profile を有効化
3. `AT+UHTTP` で `hi-corp.net` と TLS を設定
4. `AT+UHTTPC` で GET
5. `+UUHTTPCR:` を待機
6. `AT+URDFILE` でレスポンスファイルを読み出し

CA 証明書が未登録の場合は、HTTP エラー詳細を確認して CA ファイル不足の警告を表示します。

### `http_post`

`examples/http_post/http_post.ino` は、HTTP POST として稼働実績のある次の方式を使います。

1. `AT+UDWNFILE="postfile",{length}` で JSON payload をモデム内ファイルに保存
2. `AT+UHTTPC=0,4,"/","response","postfile",4` で POST
3. `+UUHTTPCR: 0,4,1` を待機
4. `AT+URDFILE="response"` でレスポンスを読み出し
5. `postfile` と `response` を削除

接続先は `https://hi-corp.net/` です。CA 証明書が未登録の場合は `http_get` と同様に警告を表示します。

## CA 証明書

HTTPS 接続では、接続先サーバーの証明書を検証するための CA 証明書がモデム側に必要です。

`https://hi-corp.net/` に接続する example では、CA ファイルが不足している場合に次の案内を表示します。

```text
The CA file required to access https://hi-corp.net/ is missing.
Please obtain the ISRG Root X1 CA file from a trusted source and install it on this modem.
You can download it from https://letsencrypt.org/certs/isrgrootx1.pem
```

CA 証明書の登録には `examples/resister_CA`、確認や削除には `examples/management_CA` を使用してください。

## デバッグ

AT コマンドの送受信ログを確認したい場合は、次を呼び出します。

```cpp
modem.enableDebugMode();
```

ログを止める場合は次を呼び出します。

```cpp
modem.disableDebugMode();
```

手動で AT コマンドを確認したい場合は、`examples/AT_Debug` を使うとシリアルモニタから直接コマンドを送信できます。

## arduino-cli でのビルド確認

このリポジトリをカレントディレクトリにして、次のように examples をコンパイルできます。

```bash
arduino-cli compile --fqbn esp32:esp32:esp32 --libraries . examples/http_get
arduino-cli compile --fqbn esp32:esp32:esp32 --libraries . examples/http_post
```

利用する ESP32 ボードに合わせて `--fqbn` は変更してください。

## ライセンス

MIT License
