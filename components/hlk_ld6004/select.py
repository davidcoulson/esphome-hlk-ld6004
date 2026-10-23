import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import select

from . import CONF_HLK_LD6004_ID, HLKLD6004, ns

LD6004Select = ns.class_("LD6004Select", select.Select, cg.Parented.template(HLKLD6004))

# type -> (kind id in C++, options in radar order)
TYPES = {
    "sensitivity": (0, ["Low", "Medium", "High"]),
    "trigger_speed": (1, ["Slow", "Medium", "Fast"]),
    "install_method": (2, ["Top", "Side"]),
    "operating_mode": (
        3,
        ["Low power", "Normal", "Off (P20 high)", "Off (P20 low)", "High reflectivity"],
    ),
    "p20_output": (
        4,
        [
            "High when present",
            "Low when present",
            "Always low",
            "Always high",
            "Pulse low",
            "Pulse high",
        ],
    ),
}

CONFIG_SCHEMA = select.select_schema(LD6004Select).extend(
    {
        cv.GenerateID(CONF_HLK_LD6004_ID): cv.use_id(HLKLD6004),
        cv.Required("type"): cv.one_of(*TYPES, lower=True),
    }
)


async def to_code(config):
    kind, options = TYPES[config["type"]]
    var = await select.new_select(config, options=options)
    cg.add(var.set_kind(kind))
    await cg.register_parented(var, config[CONF_HLK_LD6004_ID])
    cg.add((await cg.get_variable(config[CONF_HLK_LD6004_ID])).register_select(var))
