from esphome import pins
import esphome.codegen as cg
from esphome.components import i2c, touchscreen
from esphome.components.esp32 import add_idf_component
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_INTERRUPT_PIN

spd_ns = cg.esphome_ns.namespace("spd2010_touch")
SPD2010Touchscreen = spd_ns.class_("SPD2010Touchscreen", touchscreen.Touchscreen)

CONF_I2C_ID = "i2c_id"

CONFIG_SCHEMA = (
    touchscreen.touchscreen_schema(
        defaults={
            "x_min": 0, "x_max": 411,
            "y_min": 0, "y_max": 411,
            "swap_xy": False,
            "mirror_x": False,
            "mirror_y": False,
        }
    )
    .extend({
        cv.GenerateID(): cv.declare_id(SPD2010Touchscreen),
        cv.GenerateID(CONF_I2C_ID): cv.use_id(i2c.I2CBus),
        cv.Required(CONF_INTERRUPT_PIN): pins.internal_gpio_input_pin_schema,
    })
)

async def to_code(config):
    add_idf_component(name="espressif/esp_lcd_touch", ref="1.2.1")
    add_idf_component(name="espressif/esp_lcd_touch_spd2010", ref="2.0.1")
    var = cg.new_Pvariable(config[CONF_ID])
    await touchscreen.register_touchscreen(var, config)
    bus = await cg.get_variable(config[CONF_I2C_ID])
    cg.add(var.set_i2c_bus(bus))
    irq = await cg.gpio_pin_expression(config[CONF_INTERRUPT_PIN])
    cg.add(var.set_interrupt_pin(irq))
