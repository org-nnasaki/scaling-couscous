# BMP280 ドライバ仕様書

**センサー:** Bosch BMP280 — 温度・気圧センサー
**データシート:** BST-BMP280-DS001 (Rev 1.26)
**チップID:** 0x58 (レジスタ 0xD0)

> **BME280との関係:** BMP280はBME280の下位互換。レジスタ構造・I2Cプロトコル・気圧/温度補正アルゴリズムは共通。ただし**湿度機能なし**（ctrl_humレジスタ、湿度データレジスタ、湿度キャリブレーション非搭載）。

---

## 1. レジスタマップ

### 1.1 メモリマップ概要

| レジスタ名 | アドレス | bit7 | bit6 | bit5 | bit4 | bit3 | bit2 | bit1 | bit0 | リセット値 | R/W |
|---|---|---|---|---|---|---|---|---|---|---|---|
| temp_xlsb | 0xFC | temp_xlsb[7:4] | | | | 0 | 0 | 0 | 0 | 0x00 | R |
| temp_lsb | 0xFB | temp_lsb[7:0] | | | | | | | | 0x00 | R |
| temp_msb | 0xFA | temp_msb[7:0] | | | | | | | | 0x80 | R |
| press_xlsb | 0xF9 | press_xlsb[7:4] | | | | 0 | 0 | 0 | 0 | 0x00 | R |
| press_lsb | 0xF8 | press_lsb[7:0] | | | | | | | | 0x00 | R |
| press_msb | 0xF7 | press_msb[7:0] | | | | | | | | 0x80 | R |
| config | 0xF5 | t_sb[2] | t_sb[1] | t_sb[0] | filter[2] | filter[1] | filter[0] | — | spi3w_en | 0x00 | R/W |
| ctrl_meas | 0xF4 | osrs_t[2] | osrs_t[1] | osrs_t[0] | osrs_p[2] | osrs_p[1] | osrs_p[0] | mode[1] | mode[0] | 0x00 | R/W |
| status | 0xF3 | — | — | — | — | measuring | — | — | im_update | 0x00 | R |
| reset | 0xE0 | reset[7:0] | | | | | | | | 0x00 | W |
| id | 0xD0 | chip_id[7:0] | | | | | | | | 0x58 | R |
| calib25..calib00 | 0xA1…0x88 | calibration data | | | | | | | | individual | R |

> **BME280との差分:**
> - ctrl_hum (0xF2) が**存在しない**
> - hum_msb/hum_lsb (0xFD/0xFE) が**存在しない**
> - 湿度キャリブレーション (0xA1, 0xE1〜0xE7) が**存在しない**
> - t_sb設定値 '110'=2000ms, '111'=4000ms（BME280では10ms, 20ms）

### 1.2 キャリブレーションレジスタ (0x88〜0x9F)

| アドレス | パラメータ | データ型 |
|---|---|---|
| 0x88 / 0x89 | dig_T1 [7:0] / [15:8] | unsigned short |
| 0x8A / 0x8B | dig_T2 [7:0] / [15:8] | signed short |
| 0x8C / 0x8D | dig_T3 [7:0] / [15:8] | signed short |
| 0x8E / 0x8F | dig_P1 [7:0] / [15:8] | unsigned short |
| 0x90 / 0x91 | dig_P2 [7:0] / [15:8] | signed short |
| 0x92 / 0x93 | dig_P3 [7:0] / [15:8] | signed short |
| 0x94 / 0x95 | dig_P4 [7:0] / [15:8] | signed short |
| 0x96 / 0x97 | dig_P5 [7:0] / [15:8] | signed short |
| 0x98 / 0x99 | dig_P6 [7:0] / [15:8] | signed short |
| 0x9A / 0x9B | dig_P7 [7:0] / [15:8] | signed short |
| 0x9C / 0x9D | dig_P8 [7:0] / [15:8] | signed short |
| 0x9E / 0x9F | dig_P9 [7:0] / [15:8] | signed short |

### 1.3 データレジスタ

- **気圧**: 0xF7〜0xF9 (20-bit unsigned、press_msb[19:12], press_lsb[11:4], press_xlsb[3:0])
- **温度**: 0xFA〜0xFC (20-bit unsigned、temp_msb[19:12], temp_lsb[11:4], temp_xlsb[3:0])

### 1.4 制御レジスタ詳細

#### ctrl_meas (0xF4) — 温度・気圧オーバーサンプリング + モード

| osrs_t/osrs_p[2:0] | 設定 |
|---|---|
| 000 | スキップ (出力=0x80000) |
| 001 | ×1 |
| 010 | ×2 |
| 011 | ×4 |
| 100 | ×8 |
| 101〜111 | ×16 |

| mode[1:0] | モード |
|---|---|
| 00 | Sleep |
| 01, 10 | Forced |
| 11 | Normal |

#### config (0xF5) — スタンバイ時間 / IIRフィルタ

| t_sb[2:0] | tstandby (ms) |
|---|---|
| 000 | 0.5 |
| 001 | 62.5 |
| 010 | 125 |
| 011 | 250 |
| 100 | 500 |
| 101 | 1000 |
| 110 | **2000** ← BME280では10ms |
| 111 | **4000** ← BME280では20ms |

| filter[2:0] | フィルタ係数 |
|---|---|
| 000 | Off |
| 001 | 2 |
| 010 | 4 |
| 011 | 8 |
| 100〜111 | 16 |

---

## 2. 通信プロトコル手順

### 2.1 I2C

- **デバイスアドレス:** 0x76 (SDO=GND) または 0x77 (SDO=VDDIO)
- **対応速度:** Standard (100 kHz), Fast (400 kHz), High Speed

#### I2C書き込み手順
```
[S] [スレーブアドレス + W(0)] [ACK] [レジスタアドレス] [ACK] [データ] [ACK] ... [P]
```
- 複数レジスタの連続書き込みが可能（ただしアドレスは自動インクリメントしない）

#### I2C読み出し手順
```
[S] [スレーブアドレス + W(0)] [ACK] [レジスタアドレス] [ACK]
[Sr] [スレーブアドレス + R(1)] [ACK] [データ0] [ACK] ... [データN] [NACK] [P]
```
- レジスタアドレスは自動インクリメント（バーストリード対応）
- データ読み出しは0xF7〜0xFCの6バイト一括バーストリードを推奨

### 2.2 SPI

- **対応モード:** SPI Mode 00 (CPOL=0, CPHA=0) または Mode 11 (CPOL=1, CPHA=1)
- **4線/3線:** spi3w_en=0で4線、spi3w_en=1で3線

#### SPI書き込み手順
```
[CSB=Low] [0 + アドレス下位7bit] [データバイト] ... [CSB=High]
```

#### SPI読み出し手順
```
[CSB=Low] [1 + アドレス下位7bit] [データバイト0] [データバイト1] ... [CSB=High]
```
- レジスタアドレスは自動インクリメント

### 2.3 初期化手順

1. チップIDの確認: レジスタ0xD0を読み出し、**0x58**であることを確認
2. ソフトリセット: レジスタ0xE0に0xB6を書き込み
3. キャリブレーションデータの読み出し: 0x88〜0x9F (24バイト: dig_T1〜dig_P9)をバーストリード
4. config (0xF5) にスタンバイ時間とフィルタを設定
5. ctrl_meas (0xF4) に温度・気圧オーバーサンプリングとモードを設定

> **BME280との差分:** ctrl_hum (0xF2) への書き込みが不要。湿度キャリブレーションの読み出しも不要。

### 2.4 測定データ読み出し手順

1. statusレジスタ(0xF3)のmeasuringビットが0になるまで待機
2. 0xF7〜0xFCの6バイトをバーストリード
3. 生データを結合:
   - press_raw = (press_msb << 12) | (press_lsb << 4) | (press_xlsb >> 4)
   - temp_raw = (temp_msb << 12) | (temp_lsb << 4) | (temp_xlsb >> 4)
4. 補正アルゴリズムを適用

---

## 3. データ補正アルゴリズム

### 3.1 データ型定義

```c
typedef int32_t  BMP280_S32_t;
typedef uint32_t BMP280_U32_t;
typedef int64_t  BMP280_S64_t;
```

### 3.2 温度補正 (整数演算版)

```c
// 戻り値: 温度 [0.01°C単位], 例: 5123 → 51.23°C
// t_fine: 気圧補正に引き継がれるグローバル変数
BMP280_S32_t t_fine;

BMP280_S32_t bmp280_compensate_T_int32(BMP280_S32_t adc_T)
{
    BMP280_S32_t var1, var2, T;
    var1 = ((((adc_T >> 3) - ((BMP280_S32_t)dig_T1 << 1))) * ((BMP280_S32_t)dig_T2)) >> 11;
    var2 = (((((adc_T >> 4) - ((BMP280_S32_t)dig_T1)) * ((adc_T >> 4) - ((BMP280_S32_t)dig_T1))) >> 12) *
            ((BMP280_S32_t)dig_T3)) >> 14;
    t_fine = var1 + var2;
    T = (t_fine * 5 + 128) >> 8;
    return T;
}
```

### 3.3 気圧補正 (64bit整数演算版)

```c
// 戻り値: 気圧 [Q24.8形式 Pa], 例: 24674867 → 24674867/256 = 96386.2 Pa = 963.862 hPa
BMP280_U32_t bmp280_compensate_P_int64(BMP280_S32_t adc_P)
{
    BMP280_S64_t var1, var2, p;
    var1 = ((BMP280_S64_t)t_fine) - 128000;
    var2 = var1 * var1 * (BMP280_S64_t)dig_P6;
    var2 = var2 + ((var1 * (BMP280_S64_t)dig_P5) << 17);
    var2 = var2 + (((BMP280_S64_t)dig_P4) << 35);
    var1 = ((var1 * var1 * (BMP280_S64_t)dig_P3) >> 8) + ((var1 * (BMP280_S64_t)dig_P2) << 12);
    var1 = (((((BMP280_S64_t)1) << 47) + var1)) * ((BMP280_S64_t)dig_P1) >> 33;
    if (var1 == 0) {
        return 0;
    }
    p = 1048576 - adc_P;
    p = (((p << 31) - var2) * 3125) / var1;
    var1 = (((BMP280_S64_t)dig_P9) * (p >> 13) * (p >> 13)) >> 25;
    var2 = (((BMP280_S64_t)dig_P8) * p) >> 19;
    p = ((p + var1 + var2) >> 8) + (((BMP280_S64_t)dig_P7) << 4);
    return (BMP280_U32_t)p;
}
```

### 3.4 気圧補正 (32bit整数演算版・代替)

```c
// 戻り値: 気圧 [Pa], 例: 96386 → 96386 Pa = 963.86 hPa
// 精度: 典型1Pa (1σ)。高フィルタ設定時はノイズ増加。
BMP280_U32_t bmp280_compensate_P_int32(BMP280_S32_t adc_P)
{
    BMP280_S32_t var1, var2;
    BMP280_U32_t p;
    var1 = (((BMP280_S32_t)t_fine) >> 1) - (BMP280_S32_t)64000;
    var2 = (((var1 >> 2) * (var1 >> 2)) >> 11) * ((BMP280_S32_t)dig_P6);
    var2 = var2 + ((var1 * ((BMP280_S32_t)dig_P5)) << 1);
    var2 = (var2 >> 2) + (((BMP280_S32_t)dig_P4) << 16);
    var1 = (((dig_P3 * (((var1 >> 2) * (var1 >> 2)) >> 13)) >> 3) +
            ((((BMP280_S32_t)dig_P2) * var1) >> 1)) >> 18;
    var1 = ((((32768 + var1)) * ((BMP280_S32_t)dig_P1)) >> 15);
    if (var1 == 0) {
        return 0;
    }
    p = (((BMP280_U32_t)(((BMP280_S32_t)1048576) - adc_P) - (var2 >> 12))) * 3125;
    if (p < 0x80000000) {
        p = (p << 1) / ((BMP280_U32_t)var1);
    } else {
        p = (p / (BMP280_U32_t)var1) * 2;
    }
    var1 = (((BMP280_S32_t)dig_P9) * ((BMP280_S32_t)(((p >> 3) * (p >> 3)) >> 13))) >> 12;
    var2 = (((BMP280_S32_t)(p >> 2)) * ((BMP280_S32_t)dig_P8)) >> 13;
    p = (BMP280_U32_t)((BMP280_S32_t)p + ((var1 + var2 + dig_P7) >> 4));
    return p;
}
```

### 3.5 補正手順まとめ

1. **必ず温度補正を最初に実行する**（t_fineが気圧補正に必要）
2. 気圧補正を実行（t_fineを使用）

---

## 4. 電源モード・動作モード

### 4.1 モード一覧

| モード | mode[1:0] | 説明 |
|---|---|---|
| Sleep | 00 | 消費電力最小。レジスタの読み書きは可能。測定は停止。 |
| Forced | 01 or 10 | 1回だけ測定を実行し、完了後にSleepモードに戻る。 |
| Normal | 11 | 測定とスタンバイを自動的に繰り返す。 |

### 4.2 推奨初期設定例

#### 超低電力モード
- osrs_t = ×1, osrs_p = ×1
- mode = Forced
- filter = Off

#### 高解像度モード
- osrs_t = ×2, osrs_p = ×16
- mode = Normal
- t_sb = 0.5ms
- filter = 16

### 4.3 ソフトリセット

レジスタ0xE0に0xB6を書き込むことで完全なパワーオンリセットと同等のリセットを実行できる。

### 4.4 ステータス確認

| ビット | 名前 | 説明 |
|---|---|---|
| status[3] | measuring | 変換実行中=1、完了=0 |
| status[0] | im_update | NVMコピー中=1、完了=0 |
