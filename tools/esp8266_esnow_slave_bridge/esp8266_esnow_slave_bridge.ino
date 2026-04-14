#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <string.h>

#define TTL_USE_SOFTWARE_SERIAL 0
#define DEBUG_LOG_ENABLE 0

#if TTL_USE_SOFTWARE_SERIAL
#include <SoftwareSerial.h>
#endif

extern "C" {
#include <espnow.h>
#include <user_interface.h>
}

// Keep these values aligned with the ESP32-S3 sender project.
static constexpr uint8_t APP_ESPNOW_CMD_MSG_TYPE = 0x01;
static constexpr int16_t APP_COMMAND_ID_MIN = 0;
static constexpr int16_t APP_COMMAND_ID_MAX = 15;
static constexpr uint16_t APP_REMOTE_CONF_THRESHOLD_Q15 = 3000U;
static constexpr uint8_t APP_EXPECT_VAR = 1;
static constexpr bool APP_CHECK_VAR = false;

static constexpr uint8_t ESPNOW_CHANNEL = 1;

// UART parameters (aligned with your module parameter table)
static constexpr uint32_t DEBUG_BAUD = 115200;
static constexpr uint32_t TTL_BAUD = 115200;
static constexpr SerialConfig DEBUG_SERIAL_CFG = SERIAL_8N1;
static constexpr SerialConfig TTL_SERIAL_CFG = SERIAL_8N1;

static constexpr uint32_t DEDUP_WINDOW_MS = 1500;
static constexpr uint32_t MIN_FORWARD_INTERVAL_MS = 0;
static constexpr uint8_t FORWARD_SOURCE_REMOTE = 1;

// Output frame: SOF(2) + LEN(1) + PAYLOAD + XOR(1)
// PAYLOAD: ver(1), source(1), command_id(2), seq(2), prob_q15(2), ts_ms(4)
static constexpr uint8_t TTL_SOF_0 = 0xA5;
static constexpr uint8_t TTL_SOF_1 = 0x5A;
static constexpr uint8_t TTL_FRAME_VER = 0x01;
static constexpr size_t TTL_PAYLOAD_LEN = 12;

// TTL forward output configuration.
// Prefer hardware UART1 (TX only, GPIO2) to save IRAM.
#if TTL_USE_SOFTWARE_SERIAL
#ifndef D5
#define D5 14
#endif
#ifndef D6
#define D6 12
#endif
static constexpr uint8_t TTL_TX_PIN = D5;
static constexpr uint8_t TTL_RX_PIN = D6;
SoftwareSerial g_ttl_serial(TTL_RX_PIN, TTL_TX_PIN);
#else
static constexpr uint8_t TTL_TX_PIN = 2;  // GPIO2, UART1 TX
#endif

static constexpr size_t RX_QUEUE_DEPTH = 8;
static constexpr size_t DEDUP_CACHE_SIZE = 16;

#if DEBUG_LOG_ENABLE
#define DBG_PRINTF(...) Serial.printf(__VA_ARGS__)
#define DBG_PRINT(x) Serial.print(x)
#define DBG_PRINTLN(x) Serial.println(x)
#else
#define DBG_PRINTF(...) ((void)0)
#define DBG_PRINT(x) ((void)0)
#define DBG_PRINTLN(x) ((void)0)
#endif

#pragma pack(push, 1)
typedef struct {
  uint8_t var;
  uint8_t msg_type;
  uint16_t seq;
  int16_t command_id;
  uint16_t prob_q15;
  uint32_t ts_ms;
} espnow_voice_cmd_t;
#pragma pack(pop)

static_assert(sizeof(espnow_voice_cmd_t) == 12, "espnow_voice_cmd_t size mismatch");

typedef struct {
  espnow_voice_cmd_t cmd;
  uint8_t src_mac[6];
  int8_t rssi;
  uint32_t rx_ms;
} rx_packet_t;

typedef struct {
  bool valid;
  uint8_t src_mac[6];
  uint16_t seq;
  uint32_t seen_ms;
} dedup_entry_t;

volatile rx_packet_t g_rx_queue[RX_QUEUE_DEPTH];
volatile uint8_t g_rx_head = 0;
volatile uint8_t g_rx_tail = 0;

dedup_entry_t g_dedup_cache[DEDUP_CACHE_SIZE];
uint8_t g_dedup_cursor = 0;
uint32_t g_last_forward_ms = 0;

static inline bool mac_equal(const uint8_t *a, const uint8_t *b) {
  for (size_t i = 0; i < 6; ++i) {
    if (a[i] != b[i]) {
      return false;
    }
  }
  return true;
}

static void print_mac(const uint8_t *mac) {
  DBG_PRINTF("%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

static bool queue_push(const rx_packet_t &pkt) {
  bool ok = false;
  noInterrupts();
  const uint8_t next = static_cast<uint8_t>((g_rx_head + 1) % RX_QUEUE_DEPTH);
  if (next != g_rx_tail) {
    memcpy((void *)&g_rx_queue[g_rx_head], &pkt, sizeof(rx_packet_t));
    g_rx_head = next;
    ok = true;
  }
  interrupts();
  return ok;
}

static bool queue_pop(rx_packet_t &pkt) {
  bool ok = false;
  noInterrupts();
  if (g_rx_tail != g_rx_head) {
    memcpy(&pkt, (const void *)&g_rx_queue[g_rx_tail], sizeof(rx_packet_t));
    g_rx_tail = static_cast<uint8_t>((g_rx_tail + 1) % RX_QUEUE_DEPTH);
    ok = true;
  }
  interrupts();
  return ok;
}

static bool is_duplicate_and_update(const uint8_t src_mac[6], uint16_t seq, uint32_t now_ms) {
  for (size_t i = 0; i < DEDUP_CACHE_SIZE; ++i) {
    dedup_entry_t &entry = g_dedup_cache[i];
    if (!entry.valid) {
      continue;
    }
    if (!mac_equal(entry.src_mac, src_mac)) {
      continue;
    }
    if (entry.seq == seq && (now_ms - entry.seen_ms) <= DEDUP_WINDOW_MS) {
      return true;
    }
  }

  dedup_entry_t &slot = g_dedup_cache[g_dedup_cursor];
  slot.valid = true;
  memcpy(slot.src_mac, src_mac, 6);
  slot.seq = seq;
  slot.seen_ms = now_ms;
  g_dedup_cursor = static_cast<uint8_t>((g_dedup_cursor + 1) % DEDUP_CACHE_SIZE);
  return false;
}

static void write_le16(uint8_t *dst, uint16_t v) {
  dst[0] = static_cast<uint8_t>(v & 0xFF);
  dst[1] = static_cast<uint8_t>((v >> 8) & 0xFF);
}

static void write_le32(uint8_t *dst, uint32_t v) {
  dst[0] = static_cast<uint8_t>(v & 0xFF);
  dst[1] = static_cast<uint8_t>((v >> 8) & 0xFF);
  dst[2] = static_cast<uint8_t>((v >> 16) & 0xFF);
  dst[3] = static_cast<uint8_t>((v >> 24) & 0xFF);
}

static void ttl_write_bytes(const uint8_t *buf, size_t len) {
  Serial.write(buf, len);
  Serial.flush();
}

static void forward_ttl_frame(const espnow_voice_cmd_t &cmd) {
  uint8_t frame[2 + 1 + TTL_PAYLOAD_LEN + 1];
  uint8_t *payload = &frame[3];

  frame[0] = TTL_SOF_0;
  frame[1] = TTL_SOF_1;
  frame[2] = static_cast<uint8_t>(TTL_PAYLOAD_LEN); 

  payload[0] = TTL_FRAME_VER;
  payload[1] = FORWARD_SOURCE_REMOTE;
  write_le16(&payload[2], static_cast<uint16_t>(cmd.command_id));
  write_le16(&payload[4], cmd.seq);
  write_le16(&payload[6], cmd.prob_q15);
  write_le32(&payload[8], cmd.ts_ms);

  uint8_t crc = frame[2];
  for (size_t i = 0; i < TTL_PAYLOAD_LEN; ++i) {
    crc ^= payload[i];
  }
  frame[3 + TTL_PAYLOAD_LEN] = crc;

  ttl_write_bytes(frame, sizeof(frame));
}

static void process_packet(const rx_packet_t &pkt) {
  const espnow_voice_cmd_t &cmd = pkt.cmd;
  const uint32_t now_ms = millis();

  if (APP_CHECK_VAR && cmd.var != APP_EXPECT_VAR) {
    DBG_PRINTF("DROP var mismatch: var=%u expected=%u\n", cmd.var, APP_EXPECT_VAR);
    return;
  }

  if (cmd.msg_type != APP_ESPNOW_CMD_MSG_TYPE) {
    DBG_PRINTF("DROP msg_type=0x%02X expected=0x%02X\n", cmd.msg_type, APP_ESPNOW_CMD_MSG_TYPE);
    return;
  }

  if (cmd.command_id < APP_COMMAND_ID_MIN || cmd.command_id > APP_COMMAND_ID_MAX) {
    DBG_PRINTF("DROP command_id=%d out_of_range[%d,%d]\n",
               cmd.command_id, APP_COMMAND_ID_MIN, APP_COMMAND_ID_MAX);
    return;
  }

  if (cmd.prob_q15 < APP_REMOTE_CONF_THRESHOLD_Q15) {
    DBG_PRINTF("DROP low_conf: id=%d prob_q15=%u threshold=%u\n",
               cmd.command_id, cmd.prob_q15, APP_REMOTE_CONF_THRESHOLD_Q15);
    return;
  }

  if (is_duplicate_and_update(pkt.src_mac, cmd.seq, now_ms)) {
    DBG_PRINTF("DROP duplicate: id=%d seq=%u\n", cmd.command_id, cmd.seq);
    return;
  }

  if ((now_ms - g_last_forward_ms) < MIN_FORWARD_INTERVAL_MS) {
    DBG_PRINTF("DROP throttle: id=%d seq=%u delta_ms=%lu\n",
               cmd.command_id, cmd.seq, static_cast<unsigned long>(now_ms - g_last_forward_ms));
    return;
  }

  forward_ttl_frame(cmd);
  g_last_forward_ms = now_ms;

  DBG_PRINT("FWD src=");
  print_mac(pkt.src_mac);
  DBG_PRINTF(" id=%d seq=%u prob_q15=%u ts_ms=%lu rssi=%d\n",
             cmd.command_id, cmd.seq, cmd.prob_q15,
             static_cast<unsigned long>(cmd.ts_ms), pkt.rssi);
}

static void on_espnow_recv(uint8_t *src_mac, uint8_t *data, uint8_t len) {
  if (src_mac == nullptr || data == nullptr || len != sizeof(espnow_voice_cmd_t)) {
    return;
  }

  rx_packet_t pkt = {};
  memcpy(pkt.src_mac, src_mac, 6);
  memcpy(&pkt.cmd, data, sizeof(espnow_voice_cmd_t));
  pkt.rssi = 0;
  pkt.rx_ms = millis();

  if (!queue_push(pkt)) {
    DBG_PRINTLN("DROP queue full");
  }
}

static bool setup_espnow_receiver() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(10);

  wifi_promiscuous_enable(1);
  wifi_set_channel(ESPNOW_CHANNEL);
  wifi_promiscuous_enable(0);

  if (esp_now_init() != 0) {
    return false;
  }

  esp_now_set_self_role(ESP_NOW_ROLE_SLAVE);
  esp_now_register_recv_cb(on_espnow_recv);
  return true;
}

void setup() {
  Serial.begin(TTL_BAUD, TTL_SERIAL_CFG);
  Serial.setDebugOutput(false);
  delay(200);

  DBG_PRINTLN("");
  DBG_PRINTLN("ESP8266 ESPNOW->TTL bridge booting...");
  DBG_PRINTF("Config: channel=%u msg_type=0x%02X cmd_range=[%d,%d] prob_min=%u\n",
             ESPNOW_CHANNEL, APP_ESPNOW_CMD_MSG_TYPE,
             APP_COMMAND_ID_MIN, APP_COMMAND_ID_MAX, APP_REMOTE_CONF_THRESHOLD_Q15);
#if TTL_USE_SOFTWARE_SERIAL
  DBG_PRINTF("TTL(soft): tx_pin=%u rx_pin=%u baud=%lu cfg=8N1\n",
             TTL_TX_PIN, TTL_RX_PIN, static_cast<unsigned long>(TTL_BAUD));
#else
  DBG_PRINTF("TTL(hw-uart1): tx_pin=%u baud=%lu cfg=serial_config\n",
             TTL_TX_PIN, static_cast<unsigned long>(TTL_BAUD));
#endif

  if (!setup_espnow_receiver()) {
    DBG_PRINTLN("ESP-NOW init failed, restarting in 2 seconds...");
    delay(2000);
    ESP.restart();
  }

  DBG_PRINTLN("ESP-NOW receiver ready.");
}

void loop() {
  rx_packet_t pkt = {};
  if (queue_pop(pkt)) {
    process_packet(pkt);
  } else {
    delay(1);
  }
}




