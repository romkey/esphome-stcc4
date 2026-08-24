import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor
from esphome.const import CONF_ID
from . import sensor as stcc4_sensor

DEPENDENCIES = ["stcc4", "text_sensor"]

STCC4Component = stcc4_sensor.STCC4Component

CONF_SENSOR_STATUS = "sensor_status"
CONF_LAST_ERROR = "last_error"


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(STCC4Component),
            cv.Optional(CONF_SENSOR_STATUS): text_sensor.text_sensor_schema(),
            cv.Optional(CONF_LAST_ERROR): text_sensor.text_sensor_schema(),
        }
    ),
    cv.has_at_least_one_key(CONF_SENSOR_STATUS, CONF_LAST_ERROR),
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_ID])

    if status_config := config.get(CONF_SENSOR_STATUS):
        sens = await text_sensor.new_text_sensor(status_config)
        cg.add(parent.set_status_text_sensor(sens))

    if error_config := config.get(CONF_LAST_ERROR):
        sens = await text_sensor.new_text_sensor(error_config)
        cg.add(parent.set_error_text_sensor(sens))
