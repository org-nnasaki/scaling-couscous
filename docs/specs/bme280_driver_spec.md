# BME280 ドライバ仕様書

**センサー:** Bosch BME280 — 温度・湿度・気圧複合センサー
**データシート:** BST-BME280-DS002 (Rev 1.23)
**チップID:** 0x60 (レジスタ 0xD0)

---

## 1. レジスタマップ

### 1.1 メモリマップ概要

| レジスタ名 | アドレス | bit7 | bit6 | bit5 | bit4 | bit3 | bit2 | bit1 | bit0 | リセット値 | R/W |
|---|---|---|---|---|---|---|---|---|---|---|---|
| hum_lsb | 0xFE | hum_lsb[7:0] | | | | | | | | 0x00 | R |
| hum_msb | 0xFD | hum_msb[7:0] | | | | | | | | 0x80 | R |
| temp_xlsb | 0xFC | temp_xlsb[7:4] | | | | 0 | 0 | 0 | 0 | 0x00 | R |
| temp_lsb | 0xFB | temp_lsb[7:0] | | | | | | | | 0x00 | R |
| temp_msb | 0xFA | temp_msb[7:0] | | | | | | | | 0x80 | R |
| press_xlsb | 0xF9 | press_xlsb[7:4] | | | | 0 | 0 | 0 | 0 | 0x00 | R |
| press_lsb | 0xF8 | press_lsb[7:0] | | | | | | | | 0x00 | R |
| press_msb | 0xF7 | press_msb[7:0] | | | | | | | | 0x80 | R |
| config | 0xF5 | t_sb[2] | t_sb[1] | t_sb[0] | filter[2] | filter[1] | filter[0] | — | spi3w_en | 0x00 | R/W |
| ctrl_meas | 0xF4 | osrs_t[2] | osrs_t[1] | osrs_t[0] | osrs_p[2] | osrs_p[1] | osrs_p[0] | mode[1] | mode[0] | 0x00 | R/W |
| status | 0xF3 | — | — | — | — | measuring | — | — | im_update | 0x00 | R |
| ctrl_hum | 0xF2 | — | — | — | — | — | osrs_h[2] | osrs_h[1] | osrs_h[0] | 0x00 | R/W |
| reset | 0xE0 | reset[7:0] | | | | | | | | 0x00 | W |
| id | 0xD0 | chip_id[7:0] | | | | | | | | 0x60 | R |

### 1.2 キャリブレーションレジスタ

#### 温度・気圧キャリブレーション (0x88〜0x9F)

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

#### 湿度キャリブレーション (0xA1, 0xE1〜0xE7)

| アドレス | パラメータ | データ型 | 備考 |
|---|---|---|---|
| 0xA1 | dig_H1 [7:0] | unsigned char | |
| 0xE1 / 0xE2 | dig_H2 [7:0] / [15:8] | signed short | |
| 0xE3 | dig_H3 [7:0] | unsigned char | |
| 0xE4 / 0xE5[3:0] | dig_H4 [11:4] / [3:0] | signed short | 上位8bit=0xE4, 下位4bit=0xE5[3:0] |
| 0xE5[7:4] / 0xE6 | dig_H5 [3:0] / [11:4] | signed short | 下位4bit=0xE5[7:4], 上位8bit=0xE6 |
| 0xE7 | dig_H6 | signed char | |

### 1.3 データレジスタ

- **気圧**: 0xF7〜0xF9 (20-bit unsigned、press_msb[19:12], press_lsb[11:4], press_xlsb[3:0])
- **温度**: 0xFA〜0xFC (20-bit unsigned、temp_msb[19:12], temp_lsb[11:4], temp_xlsb[3:0])
- **湿度**: 0xFD〜0xFE (16-bit unsigned、hum_msb[15:8], hum_lsb[7:0])

### 1.4 制御レジスタ詳細

#### ctrl_hum (0xF2) — 湿度オーバーサンプリング

| osrs_h[2:0] | 設定 |
|---|---|
| 000 | スキップ (出力=0x8000) |
| 001 | ×1 |
| 010 | ×2 |
| 011 | ×4 |
| 100 | ×8 |
| 101〜111 | ×16 |

**注意:** ctrl_humの変更はctrl_measへの書き込み後に有効になる。

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
| 110 | 10 |
| 111 | 20 |

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
- **対応速度:** Standard (100 kHz), Fast (400 kHz), High Speed (3.4 MHz)

#### I2C書き込み手順
```
[S] [スレーブアドレス + W(0)] [ACK] [レジスタアドレス] [ACK] [データ] [ACK] ... [P]
```
- 複数レジスタの連続書き込みが可能（ただしアドレスは自動インクリメントしない）
- レジスタアドレスとデータのペアを繰り返す

#### I2C読み出し手順
```
[S] [スレーブアドレス + W(0)] [ACK] [レジスタアドレス] [ACK]
[Sr] [スレーブアドレス + R(1)] [ACK] [データ0] [ACK] [データ1] [ACK] ... [データN] [NACK] [P]
```
- レジスタアドレスは自動インクリメント（バーストリード対応）
- データ読み出しは0xF7〜0xFEの一括バーストリードを推奨

### 2.2 SPI

- **対応モード:** SPI Mode 00 (CPOL=0, CPHA=0) または Mode 11 (CPOL=1, CPHA=1)
- **4線/3線:** spi3w_en=0で4線、spi3w_en=1で3線

#### SPI書き込み手順
```
[CSB=Low] [0 + アドレス下位7bit] [データバイト] ... [CSB=High]
```
- MSBのRW bit = 0 が書き込み
- アドレスは自動インクリメントしない（ペアで送る）

#### SPI読み出し手順
```
[CSB=Low] [1 + アドレス下位7bit] [データバイト0] [データバイト1] ... [CSB=High]
```
- MSBのRW bit = 1 が読み出し
- レジスタアドレスは自動インクリメント

### 2.3 初期化手順

1. チップIDの確認: レジスタ0xD0を読み出し、0x60であることを確認
2. ソフトリセット: レジスタ0xE0に0xB6を書き込み
3. キャリブレーションデータの読み出し:
   - 0x88〜0xA1 (26バイト: dig_T1〜dig_P9, dig_H1)をバーストリード
   - 0xE1〜0xE7 (7バイト: dig_H2〜dig_H6)をバーストリード
4. ctrl_hum (0xF2) に湿度オーバーサンプリングを設定
5. config (0xF5) にスタンバイ時間とフィルタを設定
6. ctrl_meas (0xF4) に温度・気圧オーバーサンプリングとモードを設定

### 2.4 測定データ読み出し手順

1. statusレジスタ(0xF3)のmeasuringビットが0になるまで待機
2. 0xF7〜0xFEの8バイトをバーストリード
3. 生データを結合:
   - press_raw = (press_msb << 12) | (press_lsb << 4) | (press_xlsb >> 4)
   - temp_raw = (temp_msb << 12) | (temp_lsb << 4) | (temp_xlsb >> 4)
   - hum_raw = (hum_msb << 8) | hum_lsb
4. 補正アルゴリズムを適用

---

## 3. データ補正アルゴリズム

### 3.1 データ型定義

```c
typedef int32_t  BME280_S32_t;
typedef uint32_t BME280_U32_t;
typedef int64_t  BME280_S64_t;
```

### 3.2 温度補正 (整数演算版)

```c
// 戻り値: 温度 [0.01°C単位], 例: 5123 → 51.23°C
// t_fine: 気圧・湿度補正に引き継がれるグローバル変数
BME280_S32_t t_fine;

BME280_S32_t BME280_compensate_T_int32(BME280_S32_t adc_T)
{
    BME280_S32_t var1, var2, T;
    var1 = ((((adc_T >> 3) - ((BME280_S32_t)dig_T1 << 1))) * ((BME280_S32_t)dig_T2)) >> 11;
    var2 = (((((adc_T >> 4) - ((BME280_S32_t)dig_T1)) * ((adc_T >> 4) - ((BME280_S32_t)dig_T1))) >> 12) *
            ((BME280_S32_t)dig_T3)) >> 14;
    t_fine = var1 + var2;
    T = (t_fine * 5 + 128) >> 8;
    return T;
}
```

### 3.3 気圧補正 (64bit整数演算版)

```c
// 戻り値: 気圧 [Q24.8形式 Pa], 例: 24674867 → 24674867/256 = 96386.2 Pa = 963.862 hPa
BME280_U32_t BME280_compensate_P_int64(BME280_S32_t adc_P)
{
    BME280_S64_t var1, var2, p;
    var1 = ((BME280_S64_t)t_fine) - 128000;
    var2 = var1 * var1 * (BME280_S64_t)dig_P6;
    var2 = var2 + ((var1 * (BME280_S64_t)dig_P5) << 17);
    var2 = var2 + (((BME280_S64_t)dig_P4) << 35);
    var1 = ((var1 * var1 * (BME280_S64_t)dig_P3) >> 8) + ((var1 * (BME280_S64_t)dig_P2) << 12);
    var1 = (((((BME280_S64_t)1) << 47) + var1)) * ((BME280_S64_t)dig_P1) >> 33;
    if (var1 == 0) {
        return 0;
    }
    p = 1048576 - adc_P;
    p = (((p << 31) - var2) * 3125) / var1;
    var1 = (((BME280_S64_t)dig_P9) * (p >> 13) * (p >> 13)) >> 25;
    var2 = (((BME280_S64_t)dig_P8) * p) >> 19;
    p = ((p + var1 + var2) >> 8) + (((BME280_S64_t)dig_P7) << 4);
    return (BME280_U32_t)p;
}
```

### 3.4 湿度補正 (整数演算版)

```c
// 戻り値: 湿度 [Q22.10形式 %RH], 例: 47445 → 47445/1024 = 46.333 %RH
BME280_U32_t BME280_compensate_H_int32(BME280_S32_t adc_H)
{
    BME280_S32_t v_x1_u32r;
    v_x1_u32r = (t_fine - ((BME280_S32_t)76800));
    v_x1_u32r = (((((adc_H << 14) - (((BME280_S32_t)dig_H4) << 20) -
                (((BME280_S32_t)dig_H5) * v_x1_u32r)) +
                ((BME280_S32_t)16384)) >> 15) *
                (((((((v_x1_u32r * ((BME280_S32_t)dig_H6)) >> 10) *
                (((v_x1_u32r * ((BME280_S32_t)dig_H3)) >> 11) +
                ((BME280_S32_t)32768))) >> 10) +
                ((BME280_S32_t)2097152)) *
                ((BME280_S32_t)dig_H2) + 8192) >> 14));
    v_x1_u32r = (v_x1_u32r - (((((v_x1_u32r >> 15) * (v_x1_u32r >> 15)) >> 7) *
                ((BME280_S32_t)dig_H1)) >> 4));
    v_x1_u32r = (v_x1_u32r < 0 ? 0 : v_x1_u32r);
    v_x1_u32r = (v_x1_u32r > 419430400 ? 419430400 : v_x1_u32r);
    return (BME280_U32_t)(v_x1_u32r >> 12);
}
```

### 3.5 補正手順まとめ

1. **必ず温度補正を最初に実行する**（t_fineが気圧・湿度補正に必要）
2. 気圧補正を実行（t_fineを使用）
3. 湿度補正を実行（t_fineを使用）

---

## 4. 電源モード・動作モード

### 4.1 モード一覧

| モード | mode[1:0] | 説明 |
|---|---|---|
| Sleep | 00 | 消費電力最小。レジスタの読み書きは可能。測定は停止。 |
| Forced | 01 or 10 | 1回だけ測定を実行し、完了後にSleepモードに戻る。 |
| Normal | 11 | 測定とスタンバイを自動的に繰り返す。 |

### 4.2 推奨初期設定例

#### 天気モニタリング（低電力）
- osrs_h = ×1, osrs_t = ×1, osrs_p = ×1
- mode = Forced
- filter = Off

#### 室内ナビゲーション（高精度）
- osrs_h = ×1, osrs_t = ×2, osrs_p = ×16
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

NVMコピー完了後（im_update=0）にキャリブレーションデータを読み出すこと。
