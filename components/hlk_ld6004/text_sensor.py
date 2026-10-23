import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor
from esphome.const import ENTITY_CATEGORY_DIAGNOSTIC

from . import CONF_HLK_LD6004_ID, HLKLD6004

CONF_VERSION = "version"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_HLK_LD6004_ID): cv.use_id(HLKLD6004),
        cv.Optional(CONF_VERSION): text_sensor.text_sensor_schema(
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC
        ),
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_HLK_LD6004_ID])
    if conf := config.get(CONF_VERSION):
        cg.add(hub.set_version(await text_sensor.new_text_sensor(conf)))
