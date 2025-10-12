import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    CONF_CO2,
    CONF_HUMIDITY,
    CONF_ID,
    CONF_TEMPERATURE,
    DEVICE_CLASS_CARBON_DIOXIDE,
    DEVICE_CLASS_HUMIDITY,
    DEVICE_CLASS_TEMPERATURE,
    STATE_CLASS_MEASUREMENT,
    UNIT_PARTS_PER_MILLION,
    UNIT_CELSIUS,
    UNIT_PERCENT,
)
from . import STCC4Component, stcc4_ns

CONF_PRESSURE_COMPENSATION = "pressure_compensation"
CONF_ALTITUDE_COMPENSATION = "altitude_compensation"
CONF_AUTOMATIC_SELF_CALIBRATION = "automatic_self_calibration"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(STCC4Component),
        cv.Optional(CONF_CO2): sensor.sensor_schema(
            unit_of_measurement=UNIT_PARTS_PER_MILLION,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_CARBON_DIOXIDE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(
            unit_of_measurement=UNIT_CELSIUS,
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_HUMIDITY): sensor.sensor_schema(
            unit_of_measurement=UNIT_PERCENT,
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_HUMIDITY,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_PRESSURE_COMPENSATION): cv.pressure,
        cv.Optional(CONF_ALTITUDE_COMPENSATION): cv.int_range(min=0, max=10000),
    }
)


async def to_code(config):
    paren = await cg.get_variable(config[CONF_ID])

    if co2_config := config.get(CONF_CO2):
        sens = await sensor.new_sensor(co2_config)
        cg.add(paren.set_co2_sensor(sens))

    if temperature_config := config.get(CONF_TEMPERATURE):
        sens = await sensor.new_sensor(temperature_config)
        cg.add(paren.set_temperature_sensor(sens))

    if humidity_config := config.get(CONF_HUMIDITY):
        sens = await sensor.new_sensor(humidity_config)
        cg.add(paren.set_humidity_sensor(sens))

    if CONF_PRESSURE_COMPENSATION in config:
        cg.add(paren.set_pressure_compensation(config[CONF_PRESSURE_COMPENSATION]))

    if CONF_ALTITUDE_COMPENSATION in config:
        cg.add(paren.set_altitude_compensation(config[CONF_ALTITUDE_COMPENSATION]))

