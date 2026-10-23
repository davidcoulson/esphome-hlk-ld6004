import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import button

from . import CONF_HLK_LD6004_ID, HLKLD6004, ns

LD6004Button = ns.class_("LD6004Button", button.Button, cg.Parented.template(HLKLD6004))

# type -> control sub-command
TYPES = {
    "auto_interference": 0x01,  # learn current moving objects as interference; room must be empty
    "clear_interference": 0x03,
    "reset_detection_area": 0x04,  # back to the default 6x4 m area
    "reset_unmanned": 0x1A,  # force "nobody present"
    "clear_dwell": 0x25,
}

CONFIG_SCHEMA = button.button_schema(LD6004Button).extend(
    {
        cv.GenerateID(CONF_HLK_LD6004_ID): cv.use_id(HLKLD6004),
        cv.Required("type"): cv.one_of(*TYPES, lower=True),
    }
)


async def to_code(config):
    var = await button.new_button(config)
    cg.add(var.set_command(TYPES[config["type"]]))
    await cg.register_parented(var, config[CONF_HLK_LD6004_ID])
