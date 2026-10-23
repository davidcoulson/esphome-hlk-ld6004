#pragma once

#include <vector>

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/components/uart/uart.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/select/select.h"
#include "esphome/components/number/number.h"
#include "esphome/components/button/button.h"

namespace esphome {
namespace hlk_ld6004 {

static const uint8_t MAX_ZONES = 4;
static const uint8_t MAX_TARGETS = 3;

// Kind ids shared with select.py / number.py.
enum SelectKind : uint8_t { SEL_SENSITIVITY, SEL_TRIGGER_SPEED, SEL_INSTALL, SEL_OPERATING_MODE, SEL_P20 };
enum NumberKind : uint8_t { NUM_DELAY, NUM_SLEEP, NUM_INTERVAL, NUM_DWELL_LIFE, NUM_Z_MIN, NUM_Z_MAX, NUM_ZONE };
enum ZoneType : uint8_t { ZONE_DETECTION, ZONE_INTERFERENCE, ZONE_DWELL };

class HLKLD6004;

class LD6004Select : public select::Select, public Parented<HLKLD6004> {
 public:
  void set_kind(uint8_t k) { this->kind_ = k; }
  uint8_t kind() const { return this->kind_; }

 protected:
  void control(size_t index) override;
  uint8_t kind_{0};
};

class LD6004Number : public number::Number, public Parented<HLKLD6004> {
 public:
  void set_kind(uint8_t k) { this->kind_ = k; }
  void set_zone(uint8_t type, uint8_t zone, uint8_t bound) {
    this->zone_type_ = type;
    this->zone_ = zone;
    this->bound_ = bound;
  }
  uint8_t kind() const { return this->kind_; }
  uint8_t zone_type() const { return this->zone_type_; }
  uint8_t zone() const { return this->zone_; }
  uint8_t bound() const { return this->bound_; }

 protected:
  void control(float value) override;
  uint8_t kind_{0}, zone_type_{0}, zone_{0}, bound_{0};
};

class LD6004Button : public button::Button, public Parented<HLKLD6004> {
 public:
  void set_command(uint8_t c) { this->command_ = c; }

 protected:
  void press_action() override;
  uint8_t command_{0};
};

// TinyFrame: SOF(1) ID(2 BE) LEN(2 BE) TYPE(2 BE) HEAD_CKSUM(1) DATA(LEN, LE) DATA_CKSUM(1)
// Checksum is ~XOR of the covered bytes. Protocol per swherdman/HLK-LD6004 (MIT).
class HLKLD6004 : public Component, public uart::UARTDevice {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  void set_presence(binary_sensor::BinarySensor *s) { this->presence_ = s; }
  void set_moving_target(binary_sensor::BinarySensor *s) { this->moving_ = s; }
  void set_still_target(binary_sensor::BinarySensor *s) { this->still_ = s; }
  void set_zone_presence(uint8_t i, binary_sensor::BinarySensor *s) { this->zone_presence_[i] = s; }
  void set_target_count(sensor::Sensor *s) { this->target_count_ = s; }
  void set_moving_target_count(sensor::Sensor *s) { this->moving_count_ = s; }
  void set_still_target_count(sensor::Sensor *s) { this->still_count_ = s; }
  void set_target_x(uint8_t i, sensor::Sensor *s) { this->target_x_[i] = s; }
  void set_target_y(uint8_t i, sensor::Sensor *s) { this->target_y_[i] = s; }
  void set_target_z(uint8_t i, sensor::Sensor *s) { this->target_z_[i] = s; }
  void set_target_distance(uint8_t i, sensor::Sensor *s) { this->target_distance_[i] = s; }
  void set_target_angle(uint8_t i, sensor::Sensor *s) { this->target_angle_[i] = s; }
  void set_target_speed(uint8_t i, sensor::Sensor *s) { this->target_speed_[i] = s; }
  void set_version(text_sensor::TextSensor *s) { this->version_ = s; }
  void register_select(LD6004Select *s) { this->selects_.push_back(s); }
  void register_number(LD6004Number *n) { this->numbers_.push_back(n); }

  // Called by the entities.
  void send_control(uint32_t subcmd);
  void select_changed(uint8_t kind, size_t index);
  void number_changed(LD6004Number *n, float value);

 protected:
  enum State : uint8_t { WAIT_SOF, HEADER, DATA, DATA_CKSUM };

  void handle_byte_(uint8_t b);
  void process_frame_(uint16_t type, const uint8_t *data, uint16_t len);
  void process_targets_(const uint8_t *data, uint16_t len);
  void send_frame_(uint16_t type, const uint8_t *data, uint16_t len);
  void send_zone_(uint8_t type, uint8_t zone);
  void request_config_();
  void publish_select_(uint8_t kind, uint8_t index);
  void publish_number_(uint8_t kind, float value);
  void publish_zones_(uint8_t type, const uint8_t *data, uint16_t len);

  State state_{WAIT_SOF};
  uint8_t header_[8];
  uint8_t data_[128];
  uint16_t idx_{0};
  uint16_t len_{0};
  uint16_t type_{0};
  uint16_t send_id_{0};

  // Cached so one changed bound/range value can be sent with its siblings.
  float zones_[3][MAX_ZONES][6]{};
  float z_range_[2]{};

  binary_sensor::BinarySensor *presence_{nullptr}, *moving_{nullptr}, *still_{nullptr};
  binary_sensor::BinarySensor *zone_presence_[MAX_ZONES]{};
  sensor::Sensor *target_count_{nullptr}, *moving_count_{nullptr}, *still_count_{nullptr};
  sensor::Sensor *target_x_[MAX_TARGETS]{}, *target_y_[MAX_TARGETS]{}, *target_z_[MAX_TARGETS]{};
  sensor::Sensor *target_distance_[MAX_TARGETS]{}, *target_angle_[MAX_TARGETS]{}, *target_speed_[MAX_TARGETS]{};
  text_sensor::TextSensor *version_{nullptr};
  std::vector<LD6004Select *> selects_;
  std::vector<LD6004Number *> numbers_;
};

}  // namespace hlk_ld6004
}  // namespace esphome
