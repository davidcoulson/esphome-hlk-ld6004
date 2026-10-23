import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import number
from esphome.const import UNIT_METER, UNIT_SECOND

from . import CONF_HLK_LD6004_ID, HLKLD6004, ns

LD6004Number = ns.class_("LD6004Number", number.Number, cg.Parented.template(HLKLD6004))

UNIT_MS = "ms"
# type -> (kind id in C++, min, max, step, unit)
TYPES = {
    "delay_time": (0, 0, 3600, 1, UNIT_SECOND),  # presence hold after last detection
    "sleep_time": (1, 0, 500, 10, UNIT_MS),  # low-power mode only; >500 not recommended
    "output_interval": (2, 1, 1000, 1, ""),  # frame interval, x50 ms at default
    "dwell_lifecycle": (3, 0, 3600, 1, ""),  # stay-zone lifetime multiplier
    "z_min": (4, -6, 6, 0.1, UNIT_METER),
    "z_max": (5, -6, 6, 0.1, UNIT_METER),
    "zone": (6, -10, 10, 0.1, UNIT_METER),
}
ZONE_TYPES = {"detection": 0, "interference": 1, "dwell": 2}
BOUNDS = ["x_min", "x_max", "y_min", "y_max", "z_min", "z_max"]


def _validate(config):
    is_zone = config["type"] == "zone"
    for key in ("zone_type", "zone", "bound"):
        if is_zone != (key in config):
            raise cv.Invalid(f"{key} is required for, and only valid with, type: zone")
    return config


CONFIG_SCHEMA = cv.All(
    number.number_schema(LD6004Number).extend(
        {
            cv.GenerateID(CONF_HLK_LD6004_ID): cv.use_id(HLKLD6004),
            cv.Required("type"): cv.one_of(*TYPES, lower=True),
            cv.Optional("zone_type"): cv.one_of(*ZONE_TYPES, lower=True),
            cv.Optional("zone"): cv.int_range(1, 4),
            cv.Optional("bound"): cv.one_of(*BOUNDS, lower=True),
        }
    ),
    _validate,
)


async def to_code(config):
    kind, lo, hi, step, unit = TYPES[config["type"]]
    if not config.get("unit_of_measurement") and unit:
        config = {**config, "unit_of_measurement": unit}
    var = await number.new_number(config, min_value=lo, max_value=hi, step=step)
    cg.add(var.set_kind(kind))
    if kind == 6:
        cg.add(
            var.set_zone(
                ZONE_TYPES[config["zone_type"]],
                config["zone"] - 1,
                BOUNDS.index(config["bound"]),
            )
        )
    await cg.register_parented(var, config[CONF_HLK_LD6004_ID])
    cg.add((await cg.get_variable(config[CONF_HLK_LD6004_ID])).register_number(var))
