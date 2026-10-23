#include "hlk_ld6004.h"
#include "esphome/core/log.h"
#include <cmath>
#include <cstring>

namespace esphome {
namespace hlk_ld6004 {

static const char *const TAG = "hlk_ld6004";

static const uint8_t SOF = 0x01;
// Host -> radar
static const uint16_t TYPE_CONTROL = 0x0201;
static const uint16_t TYPE_SET_AREA = 0x0202;
static const uint16_t TYPE_SET_DELAY = 0x0203;
static const uint16_t TYPE_SET_Z_RANGE = 0x0204;
static const uint16_t TYPE_SET_SLEEP = 0x0205;
static const uint16_t TYPE_SET_DWELL_LIFE = 0x0206;
static const uint16_t TYPE_SET_INTERVAL = 0x0207;
static const uint16_t TYPE_FIRMWARE = 0xFFFF;
// Radar -> host
static const uint16_t TYPE_TARGET_LOCATION = 0x0A04;
static const uint16_t TYPE_PRESENCE_STATE = 0x0A0A;
static const uint16_t TYPE_INTERFERENCE = 0x0A0B;
static const uint16_t TYPE_DETECTION_AREA = 0x0A0C;
static const uint16_t TYPE_DELAY_TIME = 0x0A0D;
static const uint16_t TYPE_SENSITIVITY = 0x0A0E;
static const uint16_t TYPE_TRIGGER_SPEED = 0x0A0F;
static const uint16_t TYPE_Z_RANGE = 0x0A10;
static const uint16_t TYPE_INSTALL_METHOD = 0x0A11;
static const uint16_t TYPE_OPERATING_MODE = 0x0A12;
static const uint16_t TYPE_SLEEP_TIME = 0x0A13;
static const uint16_t TYPE_GPIO_STATUS = 0x0A15;
static const uint16_t TYPE_DWELL_AREA = 0x0A16;
static const uint16_t TYPE_DWELL_LIFECYCLE = 0x0A17;
static const uint16_t TYPE_OUTPUT_INTERVAL = 0x0A18;

static const uint16_t TARGET_RECORD_SIZE = 20;  // x,y,z float; doppler,id int32

// Select option index -> control sub-command (library enum order).
static const uint8_t OPERATING_MODE_CMD[] = {0x16, 0x17, 0x1B, 0x1C, 0x24};
static const uint8_t P20_CMD[] = {0x1D, 0x1E, 0x20, 0x21, 0x22, 0x23};
// Sub-commands that make the radar report a setting; answered by the matching 0x0Axx frame.
static const uint8_t QUERY_CMD[] = {0x02, 0x05, 0x0D, 0x11, 0x12, 0x15, 0x18, 0x19, 0x1F, 0x26, 0x27};
// SET_AREA area id = base + zone index.
static const uint8_t AREA_BASE[] = {4, 0, 8};  // detection, interference, dwell

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

static void put_u32le(uint8_t *p, uint32_t v) {
  for (int i = 0; i < 4; i++)
    p[i] = v >> (8 * i);
}

static void put_f32le(uint8_t *p, float f) {
  uint32_t v;
  memcpy(&v, &f, 4);
  put_u32le(p, v);
}

void LD6004Select::control(size_t index) {
  this->publish_state(index);
  this->parent_->select_changed(this->kind_, index);
}

void LD6004Number::control(float value) {
  this->publish_state(value);
  this->parent_->number_changed(this, value);
}

void LD6004Button::press_action() { this->parent_->send_control(this->command_); }

void HLKLD6004::setup() {
  // Give the radar a moment to boot, then read back its settings to populate the entities.
  this->set_timeout(3000, [this]() { this->request_config_(); });
}

void HLKLD6004::dump_config() {
  ESP_LOGCONFIG(TAG, "HLK-LD6004 60GHz radar:");
  LOG_BINARY_SENSOR("  ", "Presence", this->presence_);
  LOG_SENSOR("  ", "Target count", this->target_count_);
}

void HLKLD6004::loop() {
  uint8_t b;
  while (this->available() && this->read_byte(&b))
    this->handle_byte_(b);
}

// ---------------------------------------------------------------- RX

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
      if (b == (this->len_ ? checksum(this->data_, this->len_) : 0xFF)) {
        this->process_frame_(this->type_, this->data_, this->len_);
      } else {
        ESP_LOGV(TAG, "Bad data checksum, type 0x%04X", this->type_);
      }
      this->state_ = WAIT_SOF;
      break;
  }
}

void HLKLD6004::process_frame_(uint16_t type, const uint8_t *data, uint16_t len) {
  switch (type) {
    case TYPE_PRESENCE_STATE:
      if (len < 16)
        return;
      {
        bool any = false;
        for (uint8_t i = 0; i < MAX_ZONES; i++) {
          bool occ = u32le(data + i * 4) != 0;
          any |= occ;
          if (this->zone_presence_[i] != nullptr)
            this->zone_presence_[i]->publish_state(occ);
        }
        if (this->presence_ != nullptr)
          this->presence_->publish_state(any);
      }
      break;
    case TYPE_TARGET_LOCATION:
      this->process_targets_(data, len);
      break;
    case TYPE_SENSITIVITY:
      if (len >= 1)
        this->publish_select_(SEL_SENSITIVITY, data[0]);
      break;
    case TYPE_TRIGGER_SPEED:
      if (len >= 1)
        this->publish_select_(SEL_TRIGGER_SPEED, data[0]);
      break;
    case TYPE_INSTALL_METHOD:
      if (len >= 1)
        this->publish_select_(SEL_INSTALL, data[0]);
      break;
    case TYPE_OPERATING_MODE:
      if (len >= 1)
        this->publish_select_(SEL_OPERATING_MODE, data[0]);
      break;
    case TYPE_GPIO_STATUS:
      if (len >= 1)
        this->publish_select_(SEL_P20, data[0]);
      break;
    case TYPE_DELAY_TIME:
      if (len >= 4)
        this->publish_number_(NUM_DELAY, u32le(data));
      break;
    case TYPE_SLEEP_TIME:
      if (len >= 4)
        this->publish_number_(NUM_SLEEP, u32le(data));
      break;
    case TYPE_OUTPUT_INTERVAL:
      if (len >= 4)
        this->publish_number_(NUM_INTERVAL, u32le(data));
      break;
    case TYPE_DWELL_LIFECYCLE:
      if (len >= 4)
        this->publish_number_(NUM_DWELL_LIFE, u32le(data));
      break;
    case TYPE_Z_RANGE:
      if (len >= 8) {
        this->z_range_[0] = f32le(data);
        this->z_range_[1] = f32le(data + 4);
        this->publish_number_(NUM_Z_MIN, this->z_range_[0]);
        this->publish_number_(NUM_Z_MAX, this->z_range_[1]);
      }
      break;
    case TYPE_DETECTION_AREA:
      this->publish_zones_(ZONE_DETECTION, data, len);
      break;
    case TYPE_INTERFERENCE:
      this->publish_zones_(ZONE_INTERFERENCE, data, len);
      break;
    case TYPE_DWELL_AREA:
      this->publish_zones_(ZONE_DWELL, data, len);
      break;
    case TYPE_FIRMWARE:
      // Field meaning of the 4 bytes is unverified; shown as dotted bytes.
      if (len >= 4 && this->version_ != nullptr)
        this->version_->publish_state(str_sprintf("%u.%u.%u.%u", data[0], data[1], data[2], data[3]));
      break;
    default:
      break;  // ACKs and unknown frames
  }
}

void HLKLD6004::process_targets_(const uint8_t *data, uint16_t len) {
  if (len < 4)
    return;
  uint32_t count = u32le(data);
  if (count > MAX_TARGETS)
    count = MAX_TARGETS;
  if (len < 4 + count * TARGET_RECORD_SIZE)
    return;

  uint32_t moving = 0;
  for (uint8_t i = 0; i < MAX_TARGETS; i++) {
    bool have = i < count;
    const uint8_t *rec = data + 4 + i * TARGET_RECORD_SIZE;
    float x = NAN, y = NAN, z = NAN, doppler = NAN;
    if (have) {
      x = f32le(rec);
      y = f32le(rec + 4);
      z = f32le(rec + 8);
      doppler = (int32_t) u32le(rec + 12);
      moving += doppler != 0;
    }
    // Empty slot: publish NaN (unknown) rather than a stale position.
    if (this->target_x_[i] != nullptr)
      this->target_x_[i]->publish_state(x);
    if (this->target_y_[i] != nullptr)
      this->target_y_[i]->publish_state(y);
    if (this->target_z_[i] != nullptr)
      this->target_z_[i]->publish_state(z);
    if (this->target_speed_[i] != nullptr)
      this->target_speed_[i]->publish_state(doppler);
    if (this->target_distance_[i] != nullptr)
      this->target_distance_[i]->publish_state(have ? sqrtf(x * x + y * y + z * z) : NAN);
    if (this->target_angle_[i] != nullptr)  // 0 deg = straight ahead (+Y), positive towards +X
      this->target_angle_[i]->publish_state(have ? atan2f(x, y) * 180.0f / (float) M_PI : NAN);
  }

  if (this->target_count_ != nullptr)
    this->target_count_->publish_state(count);
  if (this->moving_count_ != nullptr)
    this->moving_count_->publish_state(moving);
  if (this->still_count_ != nullptr)
    this->still_count_->publish_state(count - moving);
  // "Moving" is a heuristic: a target counts as moving when its doppler index is non-zero.
  if (this->moving_ != nullptr)
    this->moving_->publish_state(moving > 0);
  if (this->still_ != nullptr)
    this->still_->publish_state(count > moving);
}

void HLKLD6004::publish_select_(uint8_t kind, uint8_t index) {
  for (auto *s : this->selects_)
    if (s->kind() == kind && s->has_index(index))
      s->publish_state((size_t) index);
}

void HLKLD6004::publish_number_(uint8_t kind, float value) {
  for (auto *n : this->numbers_)
    if (n->kind() == kind)
      n->publish_state(value);
}

void HLKLD6004::publish_zones_(uint8_t type, const uint8_t *data, uint16_t len) {
  if (len < MAX_ZONES * 24)
    return;
  for (uint8_t z = 0; z < MAX_ZONES; z++)
    for (uint8_t b = 0; b < 6; b++)
      this->zones_[type][z][b] = f32le(data + z * 24 + b * 4);
  for (auto *n : this->numbers_)
    if (n->kind() == NUM_ZONE && n->zone_type() == type)
      n->publish_state(this->zones_[type][n->zone()][n->bound()]);
}

// ---------------------------------------------------------------- TX

void HLKLD6004::send_frame_(uint16_t type, const uint8_t *data, uint16_t len) {
  this->send_id_++;
  uint8_t hdr[8] = {SOF,
                    (uint8_t) (this->send_id_ >> 8),
                    (uint8_t) this->send_id_,
                    (uint8_t) (len >> 8),
                    (uint8_t) len,
                    (uint8_t) (type >> 8),
                    (uint8_t) type,
                    0};
  hdr[7] = checksum(hdr, 7);
  this->write_array(hdr, 8);
  if (len > 0)
    this->write_array(data, len);
  this->write_byte(len > 0 ? checksum(data, len) : 0xFF);
}

void HLKLD6004::send_control(uint32_t subcmd) {
  uint8_t d[4];
  put_u32le(d, subcmd);
  this->send_frame_(TYPE_CONTROL, d, 4);
}

void HLKLD6004::send_zone_(uint8_t type, uint8_t zone) {
  uint8_t d[28];
  put_u32le(d, AREA_BASE[type] + zone);
  for (uint8_t b = 0; b < 6; b++)
    put_f32le(d + 4 + b * 4, this->zones_[type][zone][b]);
  this->send_frame_(TYPE_SET_AREA, d, 28);
}

void HLKLD6004::request_config_() {
  for (uint8_t cmd : QUERY_CMD)
    this->send_control(cmd);
  this->send_frame_(TYPE_FIRMWARE, nullptr, 0);
}

void HLKLD6004::select_changed(uint8_t kind, size_t index) {
  switch (kind) {
    case SEL_SENSITIVITY:  // 0x0A..0x0C
      this->send_control(0x0A + index);
      break;
    case SEL_TRIGGER_SPEED:  // 0x0E..0x10
      this->send_control(0x0E + index);
      break;
    case SEL_INSTALL:  // 0x13 top, 0x14 side
      this->send_control(0x13 + index);
      break;
    case SEL_OPERATING_MODE:
      if (index < sizeof(OPERATING_MODE_CMD))
        this->send_control(OPERATING_MODE_CMD[index]);
      break;
    case SEL_P20:
      if (index < sizeof(P20_CMD))
        this->send_control(P20_CMD[index]);
      break;
  }
}

void HLKLD6004::number_changed(LD6004Number *n, float value) {
  uint8_t d[8];
  switch (n->kind()) {
    case NUM_DELAY:
    case NUM_SLEEP:
    case NUM_INTERVAL:
    case NUM_DWELL_LIFE: {
      static const uint16_t TYPES[] = {TYPE_SET_DELAY, TYPE_SET_SLEEP, TYPE_SET_INTERVAL, TYPE_SET_DWELL_LIFE};
      put_u32le(d, (uint32_t) value);
      this->send_frame_(TYPES[n->kind()], d, 4);
      break;
    }
    case NUM_Z_MIN:
    case NUM_Z_MAX:
      this->z_range_[n->kind() == NUM_Z_MAX] = value;
      put_f32le(d, this->z_range_[0]);
      put_f32le(d + 4, this->z_range_[1]);
      this->send_frame_(TYPE_SET_Z_RANGE, d, 8);
      break;
    case NUM_ZONE:
      this->zones_[n->zone_type()][n->zone()][n->bound()] = value;
      this->send_zone_(n->zone_type(), n->zone());
      break;
  }
}

}  // namespace hlk_ld6004
}  // namespace esphome
