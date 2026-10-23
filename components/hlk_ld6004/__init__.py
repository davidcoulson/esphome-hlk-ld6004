import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor, sensor, uart
from esphome.const import (
    CONF_ID,
    DEVICE_CLASS_OCCUPANCY,
    STATE_CLASS_MEASUREMENT,
    UNIT_METER,
)

DEPENDENCIES = ["uart"]
AUTO_LOAD = ["binary_sensor", "sensor"]

CONF_PRESENCE = "presence"
CONF_TARGET_COUNT = "target_count"
CONF_ZONES = "zones"  # per-zone presence, zones 1-4
CONF_TARGETS = "targets"  # up to 3 tracked targets
CONF_X, CONF_Y, CONF_Z = "x", "y", "z"
MAX_ZONES = 4
MAX_TARGETS = 3

ns = cg.esphome_ns.namespace("hlk_ld6004")
HLKLD6004 = ns.class_("HLKLD6004", cg.Component, uart.UARTDevice)

_axis = sensor.sensor_schema(
    unit_of_measurement=UNIT_METER,
    accuracy_decimals=2,
    state_class=STATE_CLASS_MEASUREMENT,
)
TARGET_SCHEMA = cv.Schema(
    {cv.Optional(a): _axis for a in (CONF_X, CONF_Y, CONF_Z)}
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(HLKLD6004),
            cv.Optional(CONF_PRESENCE): binary_sensor.binary_sensor_schema(
                device_class=DEVICE_CLASS_OCCUPANCY
            ),
            cv.Optional(CONF_ZONES): cv.All(
                cv.ensure_list(
                    binary_sensor.binary_sensor_schema(
                        device_class=DEVICE_CLASS_OCCUPANCY
                    )
                ),
                cv.Length(max=MAX_ZONES),
            ),
            cv.Optional(CONF_TARGET_COUNT): sensor.sensor_schema(
                accuracy_decimals=0, state_class=STATE_CLASS_MEASUREMENT
            ),
            cv.Optional(CONF_TARGETS): cv.All(
                cv.ensure_list(TARGET_SCHEMA), cv.Length(max=MAX_TARGETS)
            ),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(uart.UART_DEVICE_SCHEMA)
)

FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema(
    "hlk_ld6004", baud_rate=115200, require_tx=False, require_rx=True
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    if conf := config.get(CONF_PRESENCE):
        cg.add(var.set_presence(await binary_sensor.new_binary_sensor(conf)))
    for i, conf in enumerate(config.get(CONF_ZONES, [])):
        cg.add(var.set_zone_presence(i, await binary_sensor.new_binary_sensor(conf)))
    if conf := config.get(CONF_TARGET_COUNT):
        cg.add(var.set_target_count(await sensor.new_sensor(conf)))
    for i, target in enumerate(config.get(CONF_TARGETS, [])):
        for axis, conf in target.items():
            s = await sensor.new_sensor(conf)
            cg.add(getattr(var, f"set_target_{axis}")(i, s))
