import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor, sensor, uart
from esphome.const import (
    CONF_ID,
    DEVICE_CLASS_OCCUPANCY,
    STATE_CLASS_MEASUREMENT,
    UNIT_DEGREES,
    UNIT_METER,
)

CODEOWNERS = ["@davidcoulson"]
DEPENDENCIES = ["uart"]
AUTO_LOAD = ["binary_sensor", "sensor", "select", "number", "button", "text_sensor"]
MULTI_CONF = True

CONF_HLK_LD6004_ID = "hlk_ld6004_id"
CONF_PRESENCE = "presence"
CONF_MOVING_TARGET = "moving_target"
CONF_STILL_TARGET = "still_target"
CONF_ZONES = "zones"  # per-zone presence, zones 1-4
CONF_TARGET_COUNT = "target_count"
CONF_MOVING_TARGET_COUNT = "moving_target_count"
CONF_STILL_TARGET_COUNT = "still_target_count"
CONF_TARGETS = "targets"  # up to 3 tracked targets
MAX_ZONES = 4
MAX_TARGETS = 3

ns = cg.esphome_ns.namespace("hlk_ld6004")
HLKLD6004 = ns.class_("HLKLD6004", cg.Component, uart.UARTDevice)

_occupancy = binary_sensor.binary_sensor_schema(device_class=DEVICE_CLASS_OCCUPANCY)
_count = sensor.sensor_schema(accuracy_decimals=0, state_class=STATE_CLASS_MEASUREMENT)


def _axis(unit, decimals=2):
    return sensor.sensor_schema(
        unit_of_measurement=unit,
        accuracy_decimals=decimals,
        state_class=STATE_CLASS_MEASUREMENT,
    )


# Setter suffix on HLKLD6004 -> schema. speed is the radar's raw doppler index, not m/s.
TARGET_FIELDS = {
    "x": _axis(UNIT_METER),
    "y": _axis(UNIT_METER),
    "z": _axis(UNIT_METER),
    "distance": _axis(UNIT_METER),
    "angle": _axis(UNIT_DEGREES, 1),
    "speed": _axis("", 0),
}

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(HLKLD6004),
            cv.Optional(CONF_PRESENCE): _occupancy,
            cv.Optional(CONF_MOVING_TARGET): _occupancy,
            cv.Optional(CONF_STILL_TARGET): _occupancy,
            cv.Optional(CONF_ZONES): cv.All(
                cv.ensure_list(_occupancy), cv.Length(max=MAX_ZONES)
            ),
            cv.Optional(CONF_TARGET_COUNT): _count,
            cv.Optional(CONF_MOVING_TARGET_COUNT): _count,
            cv.Optional(CONF_STILL_TARGET_COUNT): _count,
            cv.Optional(CONF_TARGETS): cv.All(
                cv.ensure_list(
                    cv.Schema({cv.Optional(k): v for k, v in TARGET_FIELDS.items()})
                ),
                cv.Length(max=MAX_TARGETS),
            ),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(uart.UART_DEVICE_SCHEMA)
)

FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema(
    "hlk_ld6004", baud_rate=115200, require_tx=True, require_rx=True
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    for key, setter in (
        (CONF_PRESENCE, "set_presence"),
        (CONF_MOVING_TARGET, "set_moving_target"),
        (CONF_STILL_TARGET, "set_still_target"),
    ):
        if conf := config.get(key):
            cg.add(getattr(var, setter)(await binary_sensor.new_binary_sensor(conf)))
    for i, conf in enumerate(config.get(CONF_ZONES, [])):
        cg.add(var.set_zone_presence(i, await binary_sensor.new_binary_sensor(conf)))

    for key, setter in (
        (CONF_TARGET_COUNT, "set_target_count"),
        (CONF_MOVING_TARGET_COUNT, "set_moving_target_count"),
        (CONF_STILL_TARGET_COUNT, "set_still_target_count"),
    ):
        if conf := config.get(key):
            cg.add(getattr(var, setter)(await sensor.new_sensor(conf)))
    for i, target in enumerate(config.get(CONF_TARGETS, [])):
        for field, conf in target.items():
            s = await sensor.new_sensor(conf)
            cg.add(getattr(var, f"set_target_{field}")(i, s))
