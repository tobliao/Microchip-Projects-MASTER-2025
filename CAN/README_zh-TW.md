# CAN 匯流排協定教學實驗

**硬體：** PIC32CM3204GV00048，搭載 APP-MASTERS25-1 開發板  
**開發環境：** MPLAB X IDE + XC32 編譯器 + MCC Melody

---

## 本實驗的學習目標

完成本實驗後，你將能夠：

1. **描述 CAN 訊框結構** — SOF、識別碼（Identifier）、資料長度碼（DLC）、資料欄位、CRC-15、ACK、EOF
2. **解釋 CRC-15** — 多項式錯誤偵測如何保護每一個訊框
3. **區分控制器與收發器** — 各自的功能以及為何兩者缺一不可
4. **追蹤資料編碼** — 從原始十六進位位元組解碼引擎轉速、車速與油門開度
5. **建立即時嵌入式使用者介面** — 整合多個周邊（UART、ADC、I2C OLED、GPIO）

---

## 兩分鐘認識 CAN 協定

### CAN 是什麼？

控制器局域網路（Controller Area Network，CAN）是博世（Bosch）於 1986 年為汽車應用所開發的**多主機廣播匯流排**。每個節點都能聽到所有訊息，沒有所謂的「主機」——任何節點在匯流排空閒時皆可發送。

**實體層：** 一對雙絞線（CAN_H 與 CAN_L）。  
邏輯上的**顯性（dominant）位元**會將 CAN_H 拉高、CAN_L 拉低（差動電壓約 2 V）。  
邏輯上的**隱性（recessive）位元**則讓兩條線都維持在約 2.5 V（無驅動）。

### CAN 訊框結構

```
+-----+---------------+-----+-----+-----+-------+------ - ------+--------+-----+-----+
| SOF |  Identifier   | RTR | IDE |  r0 |  DLC  |     DATA      | CRC-15 | ACK | EOF |
|  1b |     11 位元   |  1b |  1b |  1b | 4 位元|  0 – 8 位元組  | 15+1 b |  2b |  7b |
+-----+---------------+-----+-----+-----+-------+---------------+--------+-----+-----+
```

| 欄位 | 大小 | 說明 |
|------|------|------|
| **SOF** | 1 位元 | 訊框起始（Start Of Frame）——顯性位元啟動傳輸 |
| **Identifier** | 11 位元 | 訊息識別碼；**數值越小，優先權越高** |
| **RTR** | 1 位元 | 0 = 資料訊框，1 = 遠端請求訊框 |
| **DLC** | 4 位元 | 資料長度碼——資料位元組數（0–8） |
| **DATA** | 0–8 位元組 | 有效載荷 |
| **CRC-15** | 15 位元 | 循環冗餘校驗，生成多項式 `0x4599` |
| **ACK** | 2 位元 | 任何接收者將此欄位拉為顯性以確認收到 |
| **EOF** | 7 位元 | 訊框結束——七個隱性位元 |

### 本實驗展示什麼

模擬程式依序循環發送三種訊息類型：

| CAN ID | 訊息名稱 | 資料 | 解碼方式 |
|--------|---------|------|----------|
| `0x0C0` | `ENGINE_RPM` | 2 位元組 | `RPM = (Data[0] << 8) \| Data[1]` |
| `0x0D0` | `VEHICLE_SPEED` | 1 位元組 | `Speed = Data[0]`（km/h） |
| `0x0F0` | `THROTTLE_BRAKE` | 2 位元組 | `Throttle = Data[0]`（%），`Brake = Data[1]`（0/1） |

每個訊框都會在終端機上印出原始位元組、CRC-15 數值，以及解碼公式，讓你能夠手動驗算每一個數值。

### 為什麼需要 CRC-15？

CAN 的 CRC 採用生成多項式 **x¹⁵ + x¹⁴ + x¹⁰ + x⁸ + x⁷ + x⁴ + x³ + 1**（十六進位 `0x4599`），涵蓋識別碼、DLC 及所有資料位元。任何單一位元錯誤，或長度不超過 15 位元的連續錯誤，皆可保證被偵測到。`main.c` 中的程式碼完整實作了此計算：

```c
/* CAN CRC-15 — 生成多項式 0x4599 */
static uint16_t crc15(uint16_t id, uint8_t dlc, uint8_t* data) {
    uint16_t crc = 0;
    /* 處理 11 位元識別碼 */
    for(int i = 10; i >= 0; i--) {
        uint8_t n = ((id >> i) & 1) ^ ((crc >> 14) & 1);
        crc = (crc << 1) & 0x7FFF;
        if(n) crc ^= 0x4599;
    }
    /* 依序處理 DLC（4 位元）及各資料位元組 */
    ...
    return crc;
}
```

---

## 硬體實際情況

**PIC32CM3204GV00048 不具備內建 CAN 控制器。** 這一點非常重要：

```
   MCU                ATA6561          CAN 匯流排
 (PIC32CM)           收發器           （雙絞線）
+----------+       +----------+
|          |  TX   |          |  CAN_H ─────────
| 軟體模擬 |──────>| 數位訊號 |
| CAN 協定 |       | ↕        |  CAN_L ─────────
|          |<──────| 差動電壓 |
+----------+  RX   +----------+
     ↑
     此處沒有 CAN 控制器！
     協定邏輯完全在 main.c 中實作
```

| 元件 | 負責的工作 | 不負責的工作 |
|------|-----------|-------------|
| **MCU（PIC32CM）** | 執行應用程式、讀取感測器、驅動周邊 | 產生／接收真實 CAN 訊框 |
| **CAN 收發器（ATA6561）** | 數位 TX/RX ↔ 差動 CAN_H/CAN_L 電壓轉換 | 理解 CAN 協定 |
| **CAN 控制器（MCP2515）** | 訊框組裝、仲裁、錯誤處理、ACK | —（本板不包含此元件） |

**此模擬的教學價值：** 你可以即時觀察每個訊框的每一個欄位。CRC 由與真實 CAN 控制器相同的多項式計算。資料編碼方式（大端序 RPM、單位元組車速）符合標準汽車工業實作。

> 若要建立真實 CAN 節點，可透過 SPI（SERCOM1，PA12/PA13）加裝 **MCP2515** CAN 控制器。板上已配備的 ATA6561 收發器可直接與匯流排連接器相連。

---

## 硬體說明

| 元件 | MCU 腳位 | 功能 |
|------|---------|------|
| **SW1** | PB10 | 加速（按住加速） |
| **SW2** | PA15 | 煞車（按住減速） |
| **可變電阻** | PA03（Nano_AN2） | 油門控制 — ADC AIN1 |
| **LED1** | PA00 | CAN TX 指示燈（每幀閃爍一次） |
| **LED2** | PA01 | 通用指示燈 |
| **LED3** | PA10 | 通用指示燈 |
| **LED4** | PA11 | 煞車燈（SW2 按住時亮起） |
| **RGB1 LED** | PB09 / PA04 / PA05 | 車速顏色指示 — PWM1=紅、PWM2=綠、PWM3=藍 |
| **蜂鳴器** | PA14 | 音效回饋（預設關閉，見下方說明） |
| **OLED 顯示器** | PA12 / PA13 | SERCOM2 I2C，SSD1306 128×64 |
| **UART TX / RX** | PA08 / PA09 | SERCOM0，115200 baud |

> **蜂鳴器說明：** `main.c` 中的 `#define BUZZER_ENABLED 0`，預設關閉。將其改為 `1` 可啟用音效。關閉時，開機訊息顯示 `Buzzer: Disabled`。

---

## MCC Melody 設定步驟

### 步驟一：建立新專案

1. 開啟 **MPLAB X IDE**
2. File → New Project → Microchip Embedded → Standalone Project
3. 選擇裝置：**PIC32CM3204GV00048**
4. 選擇燒錄工具（PICkit、ICD4 等）
5. 選擇編譯器：**XC32**
6. 命名專案（例如 `CAN_lab`）

### 步驟二：開啟 MCC Melody

1. 點選工具列的 **MCC** 按鈕
2. 出現提示時選擇 **MCC Melody**
3. 等待 MCC 完全載入

### 步驟三：設定系統時脈

1. 在 Resource Management 中點選 **Clock Configuration**
2. 設定：
   - **OSC8M**：啟用 ✓
   - **GCLK Generator 0 Source**：OSC8M
   - **GCLK Generator 0 Divider**：1

### 步驟四：新增 UART（SERCOM0）

1. 在 **Device Resources** 展開 **Peripherals → SERCOM**
2. 點選 **SERCOM0** 旁的 **+**
3. 設定：

| 設定項目 | 數值 |
|---------|------|
| Operating Mode | USART Internal Clock |
| Baud Rate | 115200 |
| Character Size | 8 bits |
| Parity | None |
| Stop Bits | 1 |
| Transmit Pinout (TXPO) | PAD[0] |
| Receive Pinout (RXPO) | PAD[1] |

### 步驟五：新增 ADC（可變電阻）

1. 在 **Device Resources** 展開 **Peripherals → ADC**
2. 點選 **ADC** 旁的 **+**
3. 設定：

| 設定項目 | 數值 |
|---------|------|
| Select Prescaler | Peripheral clock divided by 64 |
| Select Sample Length | 4（半個 ADC 時脈週期） |
| Select Gain | 1x |
| Select Reference | 1/2 VDDANA（VDDANA > 2.0V 時使用） |
| Select Conversion Trigger | Free Run |
| **Select Positive Input** | **ADC AIN1 Pin**（PA03 = 板上 Nano_AN2） |
| Select Negative Input | Internal ground |
| Select Result Resolution | 12-bit result |

### 步驟六：設定所有 GPIO 腳位

開啟 **Pin Manager → Grid View**，依下表設定各腳位：

#### LED（GPIO 輸出）

| 腳位 | 自訂名稱 | 功能 | 方向 | 初始狀態 |
|------|---------|------|------|---------|
| PA00 | LED1 | GPIO | Output | Low |
| PA01 | LED2 | GPIO | Output | Low |
| PA10 | LED3 | GPIO | Output | Low |
| PA11 | LED4 | GPIO | Output | Low |

#### 按鍵（GPIO 輸入）

| 腳位 | 自訂名稱 | 功能 | 方向 | 上拉 |
|------|---------|------|------|------|
| PB10 | SW1 | GPIO | Input | ✓ 啟用 |
| PA15 | SW2 | GPIO | Input | ✓ 啟用 |

**按鍵設定重點：**
- Direction = **Input**
- **Input Enable（INEN）：** 方向設為 Input 時自動啟用
- **Pull Up Enable** = 勾選 ✓

#### RGB1 LED（共陰極）

| 腳位 | 自訂名稱 | 功能 | 方向 | 初始狀態 | 顏色 |
|------|---------|------|------|---------|------|
| PB09 | PWM1 | GPIO | Output | Low | 紅 |
| PA04 | PWM2 | GPIO | Output | Low | 綠 |
| PA05 | PWM3 | GPIO | Output | Low | 藍 |

**注意：** RGB1 是共陰極 LED。`HIGH = 亮`，`LOW = 滅` — 與其他主動低電位 LED 相反。

#### 蜂鳴器

| 腳位 | 自訂名稱 | 功能 | 方向 | 初始狀態 |
|------|---------|------|------|---------|
| PA14 | Buzzer | GPIO | Output | Low |

#### 可變電阻（ADC）

| 腳位 | 板上標示 | 自訂名稱 | 功能 | 模式 |
|------|---------|---------|------|------|
| PA03 | Nano_AN2 | Potentiometer | ADC_AIN1 | Analog |

**注意：** 板上標示為「Nano_AN2」（Pin 4），在 MCU 中 PA03 對應 ADC 通道 **AIN1**。

#### UART（隨 SERCOM0 自動設定）

| 腳位 | 功能 |
|------|------|
| PA08 | SERCOM0_PAD0（TX） |
| PA09 | SERCOM0_PAD1（RX） |

#### OLED I2C（SERCOM2 I2C Master）⚠️ 關鍵設定

1. 在 **Device Resources** 展開 **Peripherals → SERCOM**
2. 點選 **SERCOM2** 旁的 **+**
3. 設定 SERCOM2：

| 設定項目 | 數值 |
|---------|------|
| Select SERCOM Operation Mode | **I2C Master** |
| I2C Speed in KHz | 100 |
| SDA Hold Time | 300–600 ns |
| Enable operation in Standby mode | ☐ 不勾選 |

4. **關鍵 — 在 Pin Manager 指定腳位：**

| 腳位 | 功能 | ⚠️ 非 GPIO！ |
|------|------|------------|
| PA12 | **SERCOM2_PAD0** | 這是 SDA |
| PA13 | **SERCOM2_PAD1** | 這是 SCL |

**⚠️ 常見錯誤：** 請勿將 PA12/PA13 設定為附帶上拉電阻的「GPIO」。這兩個腳位**必須**指定給 **SERCOM2_PAD0** 和 **SERCOM2_PAD1**。若設為 GPIO，I2C 周邊無法控制腳位，初始化將永遠卡住。

### 步驟七：產生程式碼

1. 點選 **Generate** 按鈕
2. 等待程式碼產生完成
3. 關閉 MCC

### 步驟八：確認時脈設定（關鍵！）

產生後，開啟 `src/config/default/peripheral/clock/plib_clock.c`。

找到下列程式碼，若存在則**加上注釋**：

```c
// while(!(SYSCTRL_REGS->SYSCTRL_PCLKSR & SYSCTRL_PCLKSR_XOSCRDY_Msk));
```

若未加注釋，當板上未連接外部晶體時，MCU 將永遠卡在此處。

### 步驟九：編譯並燒錄

1. 點選 **Build**（錘子圖示）— 確認零錯誤
2. 點選 **Run**（播放圖示）進行燒錄

---

## 腳位設定摘要

MCC Pin Manager 應與下表完全一致：

| 腳位編號 | 腳位 ID | 自訂名稱 | 功能 | 模式 | 方向 | Latch |
|--------|--------|---------|------|------|------|-------|
| 1 | PA00 | LED1 | GPIO | Digital | Out | Low |
| 2 | PA01 | LED2 | GPIO | Digital | Out | Low |
| 4 | PA03 | Potentiometer | ADC_AIN1 | Analog | — | — |
| 13 | PA08 | — | SERCOM0_PAD0 | Digital | — | — |
| 14 | PA09 | — | SERCOM0_PAD1 | Digital | — | — |
| 15 | PA10 | LED3 | GPIO | Digital | Out | Low |
| 16 | PA11 | LED4 | GPIO | Digital | Out | Low |
| 8 | PB09 | PWM1 | GPIO | Digital | Out | Low | （RGB 紅）
| 9 | PA04 | PWM2 | GPIO | Digital | Out | Low | （RGB 綠）
| 10 | PA05 | PWM3 | GPIO | Digital | Out | Low | （RGB 藍）
| 19 | PB10 | SW1 | GPIO | Digital | In | — |
| 20 | PB11 | — | **Available** | — | — | — | ⚠️ 請勿設定！
| 21 | PA12 | — | SERCOM2_PAD0 | Digital | — | — | （OLED SDA）
| 22 | PA13 | — | SERCOM2_PAD1 | Digital | — | — | （OLED SCL）
| 23 | PA14 | Buzzer | GPIO | Digital | Out | Low |
| 24 | PA15 | SW2 | GPIO | Digital | In | — |

**⚠️ 重要：** **PB11（腳位 20）** 請留為「Available」，不要設定為 GPIO。任何對 PB11 的設定都會讓 WS2812B 全彩 LED 收到無效資料而持續亮白色。

---

## 操作說明

### 開機自我測試

開機後，韌體執行自我測試並在終端機印出：

```
========================================
        SYSTEM INITIALIZATION
========================================

  LEDs:    OK
  RGB:     OK
  Buzzer:  Disabled
  ADC:     45%
  OLED:    OK

System ready. Starting simulation...
```

自我測試期間：
- LED1–LED4 各短暫閃爍
- RGB 依序顯示紅→綠→藍
- ADC 讀取目前可變電阻位置
- OLED 顯示啟動畫面：**NCUExMICROCHIP / CAN Simulation / PIC32CM3204GV**

若 OLED 卡在 `OLED:` 不動，請參閱下方疑難排解。

### 操控方式

| 操控 | 動作 | 效果 |
|------|------|------|
| **SW1（加速）** | 按住 | 加速 — 速率取決於油門位置 |
| **SW2（煞車）** | 按住 | 煞車 — 固定減速率，LED4 亮起 |
| **可變電阻** | 旋轉 | 設定油門 0–100%（僅在按住 SW1 時影響加速率） |
| **兩鍵皆放開** | — | 滑行 — 車速緩慢下降 |

**關鍵概念：** 可變電阻控制的是*加速有多快*，而不是要不要加速。這模擬了真實車輛中油門控制引擎動力而非直接控制車速的行為。

### 油門對應加速率（SW1 按住時）

| 油門開度 | 每次更新增加車速 | 說明 |
|---------|----------------|------|
| 80–100% | +8 km/h | 全油門——急速加速 |
| 60–79% | +6 km/h | 大油門 |
| 40–59% | +4 km/h | 中油門 |
| 10–39% | +2 km/h | 小油門 |
| 0–9% | +0 km/h | 怠速——不加速 |

此邏輯實作於 `main.c` 的 `get_throttle_increment()` 函式。

### LED 指示燈說明

| LED | 腳位 | 說明 |
|-----|------|------|
| **LED1** | PA00 | 每次 CAN 訊框發送時閃爍 |
| **LED2** | PA01 | 通用（目前韌體未驅動） |
| **LED3** | PA10 | 通用（目前韌體未驅動） |
| **LED4** | PA11 | 煞車中（SW2 按住時亮起） |

本板所有 LED 均為**主動低電位**：`Clear()` = 亮，`Set()` = 滅。

### RGB1 LED 顏色（車速指示）

| 顏色 | 觸發條件 | 車速範圍 |
|------|---------|---------|
| **紅色** | 煞車中 | SW2 按住 |
| **黃色** | 高速 | > 70 km/h |
| **綠色** | 巡航 | 51–70 km/h |
| **青色** | 中速 | 31–50 km/h |
| **藍色** | 低速 | 11–30 km/h |
| **白色** | 停止/怠速 | 0–10 km/h |

### 自動演示模式

若開機時兩個按鍵皆未按下，韌體進入 **AUTO DEMO** 自動演示模式，依下列四個階段自動循環：

| 階段 | 動作 |
|------|------|
| 0 | 怠速，RPM = 850，車速 = 0 |
| 1 | 加速至 80 km/h |
| 2 | 以 80 km/h 巡航，RPM = 2200 |
| 3 | 煞車至停止，然後重複 |

按下任一按鍵立即切換至 **MANUAL**（手動）模式。

---

## 終端機輸出

以 **115200 baud** 連線（使用 MCP2221 的 COM 埠）。畫面每 200 ms 更新一次。

韌體依序循環三種 CAN 訊息，每次更新顯示其中一則並附完整解碼：

### 訊息一 — ENGINE_RPM（0x0C0）

```
============================================================
           CAN BUS PROTOCOL - EDUCATIONAL SIMULATION
============================================================

CONTROLS:
  [SW1] Hold = ACCELERATE    [SW2] Hold = BRAKE
  [POT] Turn = Set throttle level (0-100%)

------------------------------------------------------------
VEHICLE STATUS:

  Engine RPM:     2200
  Speed:            80  km/h
  Throttle:         45  %
  Brake:          Released
  Mode:           AUTO DEMO

  SW1: ---        SW2: ---

------------------------------------------------------------
CAN FRAME TRANSMITTED:

  Message:        ENGINE_RPM
  CAN ID:         0x0C0
  Data Length:    2 bytes
  Data Bytes:     0x08 0x98
  CRC-15:         0x1A3F

  Formula:        RPM = (Data[0] << 8) | Data[1]
  Calculation:    RPM = (8 x 256) + 152 = 2200

============================================================
```

### 訊息二 — VEHICLE_SPEED（0x0D0）

```
  Message:        VEHICLE_SPEED
  CAN ID:         0x0D0
  Data Length:    1 bytes
  Data Bytes:     0x50
  CRC-15:         0x0F22

  Formula:        Speed = Data[0]
  Calculation:    Speed = 80 km/h
```

### 訊息三 — THROTTLE_BRAKE（0x0F0）

```
  Message:        THROTTLE_BRAKE
  CAN ID:         0x0F0
  Data Length:    2 bytes
  Data Bytes:     0x2D 0x00
  CRC-15:         0x3C11

  Formula:        Throttle = Data[0], Brake = Data[1]
  Calculation:    Throttle = 45%, Brake = OFF (0)
```

三則訊息循環一輪後，OLED 也會同步更新。

---

## OLED 顯示器（128×64）

OLED 每 200 ms 更新一次即時儀表板：

```
+---------------------+
|   CAN BUS MONITOR   |  <- 標題（第 0 列）
|---------------------|
| RPM:                |  <- 標籤（第 2 列，6×8 字型）
|      2200           |  <- 數值（第 2 列，8×16 大字型）
| SPD: 80km/h  T: 45% |  <- 車速 + 油門（第 4 列）
| ACC:[*] BRK:[ ]     |  <- 按鍵狀態（第 5 列）
| 0C0:08 98  CRC:1A3F |  <- CAN ID:資料 + CRC（第 6 列）
|---------------------|
+---------------------+
```

| 欄位 | 說明 |
|------|------|
| **SPD** | 車速（km/h） |
| **T:** | 油門開度百分比（0–100%） |
| **ACC:[*]** | SW1 按下中（加速中） |
| **BRK:[*]** | SW2 按下中（煞車中） |
| **[ ]** | 按鍵放開 |
| **CRC:xxxx** | CAN 訊框 CRC-15 數值（十六進位） |

**啟動畫面：** `NCUExMICROCHIP`（第 1 列）/ `CAN Simulation`（第 3 列）/ `PIC32CM3204GV`（第 6 列），顯示約 1.5 秒後切換至儀表板。

---

## 疑難排解

### 「按鍵沒有作用」

1. 在 MCC 中確認 SW1（PB10）和 SW2（PA15）的設定：
   - Direction = **Input**
   - **Input Enable（INEN）** = ✓ 勾選
   - **Pull Up Enable** = ✓ 勾選
2. 重新產生程式碼並燒錄
3. 觀察開機終端機輸出——主迴圈開始前會顯示按鍵狀態

### 「RGB1 LED 顏色不變」

1. 確認 PB09（PWM1）、PA04（PWM2）、PA05（PWM3）在 MCC 中設為 GPIO Output
2. RGB1 是共陰極：`HIGH = 顏色亮起` — 與離散 LED 相反
3. 檢查 `main.c` 中 RGB 腳位的 PORT 群組：紅色在 Group 1（PB），綠色/藍色在 Group 0（PA）

### 「WS2812B LED 一直亮白色」

⚠️ 請勿在 MCC 設定 PB11（腳位 20）。必須留為 **Available**。

任何對 PB11 的 GPIO 設定都會傳送無效資料給 WS2812B LED 串。解決方法：

1. MCC → Pin Manager → 將 PB11 設為 **Available**
2. 重新產生程式碼
3. **清除晶片**（Erase chip）後重新燒錄（WS2812B 可能鎖住錯誤狀態，需重新上電）

### 「OLED 不顯示 / 初始化卡住」

**最常見原因 — 腳位設定錯誤：**

1. PA12 必須為 **SERCOM2_PAD0**（SDA），不是 GPIO
2. PA13 必須為 **SERCOM2_PAD1**（SCL），不是 GPIO

OLED 接線：

| OLED 腳位 | 板上連接 |
|----------|---------|
| GND | GND |
| VDD | 3.3 V |
| SCL | PA13（腳位 22） |
| SDA | PA12（腳位 21） |

其他確認項目：
- OLED I2C 位址必須為 **0x3C**（標準四腳模組）
- 終端機除錯訊息：`OLED: OK` — 若卡在印出 `OK` 之前，問題即為上述腳位設定

### 「沒有序列輸出」

1. 確認開啟正確的 COM 埠 — 必須是 **MCP2221** 的埠
2. Baud rate 必須為 **115200**
3. 按下板上的 **RESET** 按鈕重新觸發開機訊息

### 「LED 一直亮著」

本板 LED 為**主動低電位**。Low = 亮，High = 滅。`main.c` 中的巨集已處理此邏輯：

```c
#define LED1_ON()   LED1_Clear()   /* 拉低 = 點亮 */
#define LED1_OFF()  LED1_Set()     /* 拉高 = 熄滅 */
```

若 LED 一直亮著，請確認 MCC 腳位設定中的 **Initial State** 是否正確。

---

## 動手挑戰

這些練習鼓勵你直接修改 `main.c`，不需要額外硬體。

### 挑戰一 — 修改油門加速帶

在 `main.c` 中找到 `get_throttle_increment()`：

```c
static uint8_t get_throttle_increment(uint8_t thr) {
    if(thr >= 80) return 8;
    else if(thr >= 60) return 6;
    else if(thr >= 40) return 4;
    else if(thr >= 10) return 2;
    else return 0;
}
```

**任務：** 讓全油門（100%）產生 +12 km/h 而非 +8 km/h。在 OLED 和終端機上觀察變化。最高車速（`update()` 中上限為 180）會有什麼影響？

### 挑戰二 — 新增第四種 CAN 訊息

主迴圈目前以 `msg = 0, 1, 2` 循環。新增引擎溫度訊息：

```c
case 3: /* Engine Temperature */
    data[0] = 80 + (rpm / 100);   /* 簡易溫度模型 */
    current_can_id = 0x050;
    current_dlc = 1;
    ...
```

將 `% 3` 改為 `% 4`，觀察新訊息出現在終端機上。OBD-II 使用哪個 CAN ID 代表冷卻水溫？（提示：查閱 `doc/` 資料夾）

### 挑戰三 — 手算 CRC-15

在演示執行時，記錄一個 `ENGINE_RPM = 850` 的訊框：

- `Data[0] = 0x03`，`Data[1] = 0x52`
- `CAN ID = 0x0C0`，`DLC = 2`

根據 `crc15()` 函式的邏輯（或用 Python 腳本），自行計算預期的 CRC-15。與終端機印出的數值比對。若吻合，代表你已掌握此演算法。

---

## 快速參考卡

```
+------------------------------------------------+
|           CAN 互動實驗演示                     |
+------------------------------------------------+
|  SW1 (PB10)  | 按住 = 加速                     |
|  SW2 (PA15)  | 按住 = 煞車                     |
|  POT (PA03)  | 旋轉 = 油門 0-100%              |
+------------------------------------------------+
|  LED1 (PA00) | CAN TX 閃爍指示                 |
|  LED4 (PA11) | 煞車中                          |
|  RGB1 LED    | 車速顏色（PB09/PA04/PA05）      |
|  OLED        | 128x64 儀表板（PA12/PA13）      |
+------------------------------------------------+
|  UART：PA08/PA09，115200 baud                  |
|  CAN 訊息：0x0C0 / 0x0D0 / 0x0F0              |
|  更新間隔：200 ms                              |
+------------------------------------------------+
```

---

*硬體：PIC32CM3204GV00048，搭載 APP-MASTERS25-1 開發板*  
*教師參考資料請見 `doc/README_CAN_4_labs_arrangment.md`*  
*本文件為 [README.md](README.md) 之繁體中文版*
