#pragma once

#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace hlk_ld6004 {

static const uint8_t MAX_ZONES = 4;
static const uint8_t MAX_TARGETS = 3;

// TinyFrame: SOF(1) ID(2 BE) LEN(2 BE) TYPE(2 BE) HEAD_CKSUM(1) DATA(LEN, LE) DATA_CKSUM(1)
// Checksum is ~XOR of the covered bytes. Protocol per swherdman/HLK-LD6004 (MIT).
class HLKLD6004 : public Component, public uart::UARTDevice {
 public:
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  void set_presence(binary_sensor::BinarySensor *s) { this->presence_ = s; }
  void set_zone_presence(uint8_t i, binary_sensor::BinarySensor *s) { this->zone_presence_[i] = s; }
  void set_target_count(sensor::Sensor *s) { this->target_count_ = s; }
  void set_target_x(uint8_t i, sensor::Sensor *s) { this->target_x_[i] = s; }
  void set_target_y(uint8_t i, sensor::Sensor *s) { this->target_y_[i] = s; }
  void set_target_z(uint8_t i, sensor::Sensor *s) { this->target_z_[i] = s; }

 protected:
  enum State : uint8_t { WAIT_SOF, HEADER, DATA, DATA_CKSUM };

  void handle_byte_(uint8_t b);
  void process_frame_(uint16_t type, const uint8_t *data, uint16_t len);

  State state_{WAIT_SOF};
  uint8_t header_[8];
  uint8_t data_[128];
  uint16_t idx_{0};
  uint16_t len_{0};
  uint16_t type_{0};

  binary_sensor::BinarySensor *presence_{nullptr};
  binary_sensor::BinarySensor *zone_presence_[MAX_ZONES]{};
  sensor::Sensor *target_count_{nullptr};
  sensor::Sensor *target_x_[MAX_TARGETS]{};
  sensor::Sensor *target_y_[MAX_TARGETS]{};
  sensor::Sensor *target_z_[MAX_TARGETS]{};
};

}  // namespace hlk_ld6004
}  // namespace esphome
