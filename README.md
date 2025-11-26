### GIAI ĐOẠN 1: NÂNG CẤP PHẦN CỨNG (Đấu dây)

Trước khi viết bất kỳ dòng code nào, bạn cần kết nối tất cả phần cứng lại với nhau.

### 1. Nối Module Relay 2 Kênh (Shop 2)

- **VCC** (Relay) -> **VIN** (ESP32) (Để lấy nguồn 5V)
- **GND** (Relay) -> **GND** (ESP32)
- **IN1** (Relay) -> **D27** (ESP32) (Đây sẽ là Ổ Cắm 1)
- **IN2** (Relay) -> **D26** (ESP32) (Đây sẽ là Ổ Cắm 2)
- **`*Lưu ý: Bạn vẫn phải gạt 2 jumper màu vàng trên module relay sang chế độ "L" (Low Trigger) như đã bàn.*`**

### 2. Nối Micro INMP441

Đây là micro kỹ thuật số (I2S), bạn cần nối 3 chân tín hiệu:

- **VDD** (Mic) -> **3V3** (ESP32) (Quan trọng: Không phải 5V)
- **GND** (Mic) -> **GND** (ESP32)
- **SCK** (Mic) -> **D25** (ESP32) (Chân Clock của I2S)
- **WS** (Mic) -> **D33** (ESP32) (Chân Word Select của I2S)
- **SD** (Mic) -> **D32** (ESP32) (Chân Data của I2S)