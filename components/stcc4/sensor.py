import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor, i2c
from esphome.const import (
    CONF_CO2,
    CONF_HUMIDITY,
    CONF_ID,
    CONF_TEMPERATURE,
    CONF_UPDATE_INTERVAL,
    DEVICE_CLASS_CARBON_DIOXIDE,
    DEVICE_CLASS_HUMIDITY,
    DEVICE_CLASS_TEMPERATURE,
    STATE_CLASS_MEASUREMENT,
    UNIT_PARTS_PER_MILLION,
    UNIT_CELSIUS,
    UNIT_PERCENT,
)
from esphome.core import coroutine
from . import stcc4_ns

DEPENDENCIES = ["i2c"]

STCC4Component = stcc4_ns.class_("STCC4Component", cg.PollingComponent, i2c.I2CDevice)
MeasurementMode = stcc4_ns.enum("MeasurementMode")

CONF_PRESSURE_COMPENSATION = "pressure_compensation"
CONF_ALTITUDE_COMPENSATION = "altitude_compensation"
CONF_SELF_TEST_ON_BOOT = "self_test_on_boot"
CONF_SOFT_RESET_ON_BOOT = "soft_reset_on_boot"
CONF_MEASUREMENT_MODE = "measurement_mode"
CONF_SLEEP_AFTER_MEASUREMENT = "sleep_after_measurement"
CONF_RHT_COMPENSATION = "rht_compensation"

MEASUREMENT_MODES = {
    "continuous": MeasurementMode.CONTINUOUS,
    "single_shot": MeasurementMode.SINGLE_SHOT,
}

RHT_COMPENSATION_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_TEMPERATURE): cv.use_id(sensor.Sensor),
        cv.Required(CONF_HUMIDITY): cv.use_id(sensor.Sensor),
    }
)


def validate_update_interval(config):
    interval = config[CONF_UPDATE_INTERVAL]
    if interval.total_milliseconds < 1000:
        raise cv.Invalid(
            "update_interval must be at least 1s (STCC4 samples every 1 second)",
            path=[CONF_UPDATE_INTERVAL],
        )
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(STCC4Component),
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
            cv.Exclusive(CONF_PRESSURE_COMPENSATION, "compensation"): cv.All(
                cv.float_,
                cv.Range(min=40000, max=110000),
            ),
            cv.Exclusive(CONF_ALTITUDE_COMPENSATION, "compensation"): cv.int_range(
                min=0, max=10000
            ),
            cv.Optional(CONF_SELF_TEST_ON_BOOT, default=True): cv.boolean,
            cv.Optional(CONF_SOFT_RESET_ON_BOOT, default=True): cv.boolean,
            cv.Optional(CONF_MEASUREMENT_MODE, default="continuous"): cv.enum(
                MEASUREMENT_MODES, lower=True
            ),
            cv.Optional(CONF_SLEEP_AFTER_MEASUREMENT, default=False): cv.boolean,
            cv.Optional(CONF_RHT_COMPENSATION): RHT_COMPENSATION_SCHEMA,
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x64)),
    validate_update_interval,
)


@coroutine
async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    if co2_config := config.get(CONF_CO2):
        sens = await sensor.new_sensor(co2_config)
        cg.add(var.set_co2_sensor(sens))

    if temperature_config := config.get(CONF_TEMPERATURE):
        sens = await sensor.new_sensor(temperature_config)
        cg.add(var.set_temperature_sensor(sens))

    if humidity_config := config.get(CONF_HUMIDITY):
        sens = await sensor.new_sensor(humidity_config)
        cg.add(var.set_humidity_sensor(sens))

    if CONF_PRESSURE_COMPENSATION in config:
        cg.add(var.set_pressure_compensation(config[CONF_PRESSURE_COMPENSATION]))

    if CONF_ALTITUDE_COMPENSATION in config:
        cg.add(var.set_altitude_compensation(config[CONF_ALTITUDE_COMPENSATION]))

    cg.add(var.set_self_test_on_boot(config[CONF_SELF_TEST_ON_BOOT]))
    cg.add(var.set_soft_reset_on_boot(config[CONF_SOFT_RESET_ON_BOOT]))
    cg.add(var.set_measurement_mode(config[CONF_MEASUREMENT_MODE]))
    cg.add(var.set_sleep_after_measurement(config[CONF_SLEEP_AFTER_MEASUREMENT]))

    if rht_config := config.get(CONF_RHT_COMPENSATION):
        temp = await cg.get_variable(rht_config[CONF_TEMPERATURE])
        hum = await cg.get_variable(rht_config[CONF_HUMIDITY])
        cg.add(var.set_rht_temperature_source(temp))
        cg.add(var.set_rht_humidity_source(hum))
