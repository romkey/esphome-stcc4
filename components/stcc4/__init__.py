import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c
from esphome.const import CONF_ID

DEPENDENCIES = ["i2c"]
AUTO_LOAD = ["sensor"]

stcc4_ns = cg.esphome_ns.namespace("stcc4")
STCC4Component = stcc4_ns.class_("STCC4Component", cg.PollingComponent, i2c.I2CDevice)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(STCC4Component),
    }
).extend(cv.polling_component_schema("60s")).extend(i2c.i2c_device_schema(0x64))


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

