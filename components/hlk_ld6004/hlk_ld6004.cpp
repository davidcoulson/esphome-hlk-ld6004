#include "hlk_ld6004.h"
#include "esphome/core/log.h"
#include <cmath>
#include <cstring>

namespace esphome {
namespace hlk_ld6004 {

static const char *const TAG = "hlk_ld6004";

static const uint8_t SOF = 0x01;
static const uint16_t TYPE_TARGET_LOCATION = 0x0A04;
static const uint16_t TYPE_PRESENCE_STATE = 0x0A0A;
static const uint16_t TARGET_RECORD_SIZE = 20;  // x,y,z float; doppler,id int32

static uint8_t checksum(const uint8_t *d, uint16_t n) {
  uint8_t r = 0;
  for (uint16_t i = 0; i < n; i++)
    r ^= d[i];
  return ~r;
}

static uint32_t u32le(const uint8_t *p) {
  return (uint32_t) p[0] | ((uint32_t) p[1] << 8) | ((uint32_t) p[2] << 16) | ((uint32_t) p[3] << 24);
}

static float f32le(const uint8_t *p) {
  uint32_t v = u32le(p);
  float f;
  memcpy(&f, &v, 4);
  return f;
}

void HLKLD6004::dump_config() {
  ESP_LOGCONFIG(TAG, "HLK-LD6004 60GHz radar:");
  LOG_BINARY_SENSOR("  ", "Presence", this->presence_);
  LOG_SENSOR("  ", "Target count", this->target_count_);
  this->check_uart_settings(115200);
}

void HLKLD6004::loop() {
  uint8_t b;
  while (this->available() && this->read_byte(&b))
    this->handle_byte_(b);
}

void HLKLD6004::handle_byte_(uint8_t b) {
  switch (this->state_) {
    case WAIT_SOF:
      if (b == SOF) {
        this->header_[0] = b;
        this->idx_ = 1;
        this->state_ = HEADER;
      }
      break;
    case HEADER:
      this->header_[this->idx_++] = b;
      if (this->idx_ < 8)
        break;
      if (this->header_[7] != checksum(this->header_, 7)) {
        ESP_LOGV(TAG, "Bad header checksum");
        this->state_ = WAIT_SOF;
        break;
      }
      this->len_ = (this->header_[3] << 8) | this->header_[4];
      this->type_ = (this->header_[5] << 8) | this->header_[6];
      this->idx_ = 0;
      if (this->len_ == 0)
        this->state_ = DATA_CKSUM;
      else if (this->len_ > sizeof(this->data_))
        this->state_ = WAIT_SOF;
      else
        this->state_ = DATA;
      break;
    case DATA:
      this->data_[this->idx_++] = b;
      if (this->idx_ == this->len_)
        this->state_ = DATA_CKSUM;
      break;
    case DATA_CKSUM:
      // Empty frames (ACKs) carry 0xFF, which is checksum() of zero bytes.
      if (b == (this->len_ ? checksum(this->data_, this->len_) : 0xFF))
        this->process_frame_(this->type_, this->data_, this->len_);
      else
        ESP_LOGV(TAG, "Bad data checksum, type 0x%04X", this->type_);
      this->state_ = WAIT_SOF;
      break;
  }
}

void HLKLD6004::process_frame_(uint16_t type, const uint8_t *data, uint16_t len) {
  if (type == TYPE_PRESENCE_STATE && len >= 16) {
    bool any = false;
    for (uint8_t i = 0; i < MAX_ZONES; i++) {
      bool occ = u32le(data + i * 4) != 0;
      any |= occ;
      if (this->zone_presence_[i] != nullptr)
        this->zone_presence_[i]->publish_state(occ);
    }
    if (this->presence_ != nullptr)
      this->presence_->publish_state(any);
  } else if (type == TYPE_TARGET_LOCATION && len >= 4) {
    uint32_t count = u32le(data);
    if (count > MAX_TARGETS)
      count = MAX_TARGETS;
    if (len < 4 + count * TARGET_RECORD_SIZE)
      return;
    if (this->target_count_ != nullptr)
      this->target_count_->publish_state(count);
    for (uint8_t i = 0; i < MAX_TARGETS; i++) {
      const uint8_t *rec = data + 4 + i * TARGET_RECORD_SIZE;
      bool have = i < count;
      // No target in slot: publish NaN (unknown) rather than a stale position.
      if (this->target_x_[i] != nullptr)
        this->target_x_[i]->publish_state(have ? f32le(rec) : NAN);
      if (this->target_y_[i] != nullptr)
        this->target_y_[i]->publish_state(have ? f32le(rec + 4) : NAN);
      if (this->target_z_[i] != nullptr)
        this->target_z_[i]->publish_state(have ? f32le(rec + 8) : NAN);
    }
  }
}

}  // namespace hlk_ld6004
}  // namespace esphome
