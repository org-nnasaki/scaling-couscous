# GitHub Copilot CLI デモプラン: ドライバ仕様書からセンサードライバを並列生成

## 概要

組み込みC++開発者向けに、GitHub Copilot CLIのAgent機能のパワーを見せるデモ。
**データシートから抽出したドライバ仕様Markdown**を入力として、ドライバコードを自動生成する実践的なシナリオ。

実際の組み込み開発では、データシートにはハードウェア特性（電気的仕様、タイミング図、パッケージ情報等）が大量に含まれるが、ドライバ開発に必要なのは**SOCのペリフェラル仕様とドライバ作成に直結する情報**だけ。このデモでは、現場のワークフローに合わせて「データシートから必要な情報を抽出した仕様書」を用意し、それを元にドライバを生成する。

**ターゲット:** 組み込みC++開発者
**C++バージョン:** C++17
**所要時間:** 20〜25分
**生成するセンサー:** 温度（BME280）、加速度（ADXL345）、気圧（BMP280）

## 仕様書の構造

### 2層構造: データシートPDF → ドライバ仕様Markdown

```
docs/
├── datasheets/               ← 元のデータシートPDF（参考資料）
│   ├── bosch/
│   │   ├── BST-BME280-DS002.pdf
│   │   └── BST-BMP280-DS001.pdf
│   └── adi/
│       └── ADXL345.pdf
└── specs/                    ← ★ドライバ仕様Markdown（実際の入力）
    ├── bme280_driver_spec.md
    ├── bmp280_driver_spec.md
    └── adxl345_driver_spec.md
```

**ポイント:**
- データシートPDFは「参考資料」として`docs/datasheets/`に保持
- ドライバ開発に必要な情報だけを抽出した`docs/specs/*.md`が**実際のコード生成入力**
- これは現場の開発者が「データシートを読んでから設計メモを書く」ワークフローと同じ

### ドライバ仕様Markdownの内容

各仕様Markdownには以下の4セクションを含める：

#### 1. レジスタマップ
- レジスタアドレス、ビットフィールド定義、リード/ライト属性
- チップID（DEVID）レジスタ
- キャリブレーションデータ格納レジスタ

#### 2. 通信プロトコル手順
- I2C/SPIデバイスアドレスとアクセス手順
- バーストリード/ライト手順
- コマンドシーケンス（初期化、測定開始、データ読み出し）

#### 3. データ補正アルゴリズム
- 温度/気圧/加速度の補正計算式（整数演算版）
- キャリブレーション係数の読み出しと適用手順
- データフォーマット（ビット幅、符号、エンディアン）

#### 4. 電源モード・動作モード
- スリープ / ノーマル / 強制測定 等のモード遷移
- オーバーサンプリング設定
- フィルタ / レンジ設定

### 各センサー仕様の概要

| 仕様ファイル | センサー | 通信 | 主要レジスタ | チップID |
|---|---|---|---|---|
| `bme280_driver_spec.md` | BME280 | I2C (0x76/0x77) | 0x88〜キャリブ, 0xFA〜データ | 0x60 |
| `bmp280_driver_spec.md` | BMP280 | I2C (0x76/0x77) | 0x88〜キャリブ, 0xFA〜データ | 0x58 |
| `adxl345_driver_spec.md` | ADXL345 | SPI (4線) | 0x00=DEVID, 0x32〜データ | 0xE5 |

**Bosch BME280/BMP280の関係:**
- I2Cプロトコル、レジスタ構造、パワーモード制御は共通
- BME280は温度+湿度+気圧、BMP280は温度+気圧のみ（湿度なし）
- 補正アルゴリズムの構造は同じだが、BME280には湿度補正が追加
- チップIDが異なる（BME280=0x60, BMP280=0x58）

### データシートの入手先（参考）

| ファイル名 | センサー | 入手元 |
|---|---|---|
| BST-BME280-DS002.pdf | BME280 | [Bosch Sensortec](https://www.bosch-sensortec.com/products/environmental-sensors/humidity-sensors-bme280/) |
| BST-BMP280-DS001.pdf | BMP280 | [Bosch Sensortec](https://www.bosch-sensortec.com/products/environmental-sensors/pressure-sensors/bmp280/) |
| ADXL345.pdf | ADXL345 | [Analog Devices](https://www.analog.com/en/products/adxl345.html) |

## デモの流れ（6ステップ）

### Step 0: 事前準備（デモ前）

データシートPDFとドライバ仕様Markdownを事前配置する。

```bash
# データシートPDF（参考資料）
docs/datasheets/bosch/BST-BME280-DS002.pdf
docs/datasheets/bosch/BST-BMP280-DS001.pdf
docs/datasheets/adi/ADXL345.pdf

# ドライバ仕様Markdown（コード生成入力）
docs/specs/bme280_driver_spec.md
docs/specs/bmp280_driver_spec.md
docs/specs/adxl345_driver_spec.md
```

**デモトーク:** 「今日のデモでは、空のプロジェクトにセンサーのドライバ仕様書だけが置いてある状態からスタートします。実際の組み込み開発では、データシートは何十ページもあってハードウェア特性が中心ですが、ドライバ開発に本当に必要なのはレジスタマップ、通信手順、補正アルゴリズム、動作モードの4つです。それだけを抽出した仕様書を用意しました。これをCopilotに読ませてドライバを書かせます」

### Step 1: プロジェクト基盤の生成（逐次・約3分）

最初にプロジェクトの骨格とベースクラスを生成する。

生成するもの:
- `CMakeLists.txt` — プロジェクトルートのビルド設定（GoogleTest統合含む）
- `include/sensor_base.hpp` — センサーの抽象基底クラス（ISensor インターフェース）
- `include/i2c_interface.hpp` — I2C HAL抽象化
- `include/spi_interface.hpp` — SPI HAL抽象化
- `src/` と `tests/` ディレクトリ構造

**デモトーク:** 「まずプロジェクトの土台を作ります。CopilotにC++17の組み込みセンサーライブラリを作りたいと伝えるだけで、CMake設定からインターフェース設計まで一気に出てきます」

### Step 2: 仕様書を読んでドライバを並列生成（★メインの見せ場・約5分）

**ここが最大のハイライト。** ドライバ仕様Markdownを入力として、3つのサブエージェントが各ドライバを並列生成する。

並列タスクA — 温度センサー（BME280）:
- **入力仕様**: `docs/specs/bme280_driver_spec.md`
- **生成**: `include/drivers/temperature_sensor.hpp`, `src/drivers/temperature_sensor.cpp`
- 仕様から反映する情報: レジスタマップ（0x88〜のキャリブレーション、0xFA〜の温度データ）、チップID（0x60）、補正アルゴリズム（整数演算版）、I2C初期化手順

並列タスクB — 加速度センサー（ADXL345）:
- **入力仕様**: `docs/specs/adxl345_driver_spec.md`
- **生成**: `include/drivers/accelerometer.hpp`, `src/drivers/accelerometer.cpp`
- 仕様から反映する情報: レジスタマップ（DEVID=0x00, DATAX0=0x32〜）、SPI通信手順、データフォーマット（10bit/13bit 2の補数）、レンジ設定

並列タスクC — 気圧センサー（BMP280）:
- **入力仕様**: `docs/specs/bmp280_driver_spec.md`
- **生成**: `include/drivers/pressure_sensor.hpp`, `src/drivers/pressure_sensor.cpp`
- 仕様から反映する情報: BME280との共通レジスタマップ、気圧補正アルゴリズム、チップID（0x58）、動作モード制御

**デモトーク:** 「ここからが面白いところです。docs/specsにあるドライバ仕様書を読ませて、3つのドライバを**同時に**生成します。仕様書にはレジスタマップや補正アルゴリズムが正確に記述されているので、Copilotはそれをそのまま実装に反映します。特にBoschのBME280とBMP280は共通のレジスタ構造を持ちますが、チップIDや対応機能の違いを各エージェントが正しく処理します」

### Step 3: ユニットテストを並列生成（約3分）

3つのセンサーそれぞれのユニットテストを並列サブエージェントで同時生成。

並列タスクA:
- `tests/test_temperature_sensor.cpp` — MockI2Cを使ったテスト

並列タスクB:
- `tests/test_accelerometer.cpp` — MockSPIを使ったテスト

並列タスクC:
- `tests/test_pressure_sensor.cpp` — MockI2Cを使ったテスト

**デモトーク:** 「テストも並列で生成します。仕様書に記載されているサンプル値やキャリブレーションデータを使ったテストケースが自動生成されます」

### Step 4: ドキュメントとREADMEを並列生成（約2分）

並列タスクA:
- `README.md` — プロジェクト全体の説明、ビルド手順、使用例

並列タスクB:
- `docs/api_reference.md` — 各センサーAPIリファレンス

並列タスクC:
- `examples/main.cpp` — 全センサーを使ったサンプルコード

**デモトーク:** 「ドキュメントもサンプルコードも並列で生成します」

### Step 5: ビルドとテスト実行で検証（約3分）

- CMakeでビルド
- GoogleTestでテスト実行
- 全テストがパスすることを確認

**デモトーク:** 「最後に、仕様書から生成したコードが実際にビルドできてテストが通ることを確認しましょう。ドライバ仕様書だけから、約20分でテスト付きのセンサードライバライブラリが完成しました」

## デモ用プロンプト案

### Step 1 のプロンプト:
```
C++17の組み込みセンサードライバライブラリのプロジェクトを作成してください。
CMakeLists.txt、センサーの抽象基底クラス(ISensor)、I2CとSPIのHAL抽象化インターフェースを生成してください。
GoogleTestを使ったテスト環境も設定してください。
```

### Step 2 のプロンプト:
```
docs/specs/ にセンサーのドライバ仕様書（Markdown）があります。
これらを読み込んで、以下の3つのセンサードライバを並列サブエージェント(autopilot_fleet)で同時に生成してください:

1. 温度センサー — docs/specs/bme280_driver_spec.md を元に実装
2. 加速度センサー — docs/specs/adxl345_driver_spec.md を元に実装
3. 気圧センサー — docs/specs/bmp280_driver_spec.md を元に実装

それぞれ include/drivers/ と src/drivers/ にヘッダとソースを生成してください。
sensor_base.hppのISensorを継承し、i2c_interface.hpp/spi_interface.hppを使ってください。
仕様書のレジスタマップ、補正アルゴリズム、チップID、動作モードを正確に反映してください。
```

### Step 3 のプロンプト:
```
生成した3つのセンサードライバそれぞれのユニットテストを並列サブエージェントで生成してください。
MockI2C/MockSPIを使い、GoogleTestで書いてください。tests/ディレクトリに配置してください。
仕様書に記載のサンプル値・キャリブレーションデータを使ったテストケースを含めてください。
```

### Step 4 のプロンプト:
```
以下を並列サブエージェントで生成してください:
1. README.md（プロジェクト概要、ビルド手順、使用例）
2. docs/api_reference.md（各センサーのAPIリファレンス）
3. examples/main.cpp（全センサーを使ったサンプルコード）
```

### Step 5 のプロンプト:
```
CMakeでビルドして、GoogleTestのテストを実行してください。
```

## プロジェクト構造（最終形）

```
20260226/
├── CMakeLists.txt
├── README.md
├── include/
│   ├── sensor_base.hpp
│   ├── i2c_interface.hpp
│   ├── spi_interface.hpp
│   └── drivers/
│       ├── temperature_sensor.hpp
│       ├── accelerometer.hpp
│       └── pressure_sensor.hpp
├── src/
│   └── drivers/
│       ├── temperature_sensor.cpp
│       ├── accelerometer.cpp
│       └── pressure_sensor.cpp
├── tests/
│   ├── test_temperature_sensor.cpp
│   ├── test_accelerometer.cpp
│   └── test_pressure_sensor.cpp
├── examples/
│   └── main.cpp
└── docs/
    ├── api_reference.md
    ├── specs/               ← ★ドライバ仕様書（コード生成入力）
    │   ├── bme280_driver_spec.md
    │   ├── bmp280_driver_spec.md
    │   └── adxl345_driver_spec.md
    └── datasheets/          ← 元のデータシートPDF（参考資料）
        ├── bosch/
        │   ├── BST-BME280-DS002.pdf
        │   └── BST-BMP280-DS001.pdf
        └── adi/
            └── ADXL345.pdf
```

## 見せ場のポイント

1. **現場のワークフロー再現**: 「データシート → 仕様書抽出 → ドライバ実装」という実際の開発フローをデモで再現
2. **焦点の明確さ**: ハードウェア特性ではなくSOCペリフェラル仕様・ドライバ実装に必要な情報だけに絞った仕様書
3. **並列生成のビジュアル**: サブエージェントが3つ同時に動く様子
4. **組み込みらしさ**: レジスタマップ、キャリブレーション、補正アルゴリズム、動作モードが仕様書通り
5. **共通仕様の理解**: BME280/BMP280が共通レジスタ構造を持つことをCopilotが理解している様子
6. **実用性**: 生成されたコードが実際にビルド・テストできること
7. **再現性**: Markdownなので入力内容が明確で、デモの再現性が高い（PDF読み取り精度に依存しない）

## 注意事項

- ドライバ仕様MarkdownとデータシートPDFは事前に配置しておく（デモ中のダウンロードは不要）
- GoogleTestはCMakeのFetchContentで自動取得する設定にする（事前インストール不要）
- 実機HALはモック/スタブなので、テスト環境だけで完結する
- BME280とBMP280は共通部分（I2Cプロトコル、レジスタ構造）があるが、チップIDや対応機能が異なることに注意
- デモ中にビルドエラーが出た場合は「Copilotに修正させる」ところも見せると良い
- Markdownを入力にすることでPDF読み取り精度のリスクを排除できる（デモの安定性向上）

## 事前作業（デモ準備として必要な作業）

1. **ドライバ仕様Markdownの作成**: 各データシートPDFから、上記4セクション（レジスタマップ、通信プロトコル、補正アルゴリズム、動作モード）を抽出してMarkdownにまとめる
2. **デモリハーサル**: 仕様Markdownを入力にしてStep 1〜5を通しで実行し、生成コードの品質を確認する
