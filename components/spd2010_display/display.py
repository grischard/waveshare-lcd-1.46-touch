from esphome import pins
import esphome.codegen as cg
from esphome.components import display
from esphome.components.esp32 import add_idf_component
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_RESET_PIN

spd_ns = cg.esphome_ns.namespace("spd2010_display")
SPD2010Display = spd_ns.class_("SPD2010Display", display.Display)

CONF_CLOCK_PIN = "clock_pin"
CONF_DATA_PINS = "data_pins"
CONF_CS_PIN = "cs_pin"
CONF_DATA_RATE_MHZ = "data_rate_mhz"

_BASE_SCHEMA = (
    display.FULL_DISPLAY_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(SPD2010Display),
            cv.Required(CONF_CLOCK_PIN): pins.internal_gpio_output_pin_schema,
            cv.Required(CONF_DATA_PINS): cv.All(
                cv.ensure_list(pins.internal_gpio_output_pin_schema),
                cv.Length(min=4, max=4),
            ),
            cv.Required(CONF_CS_PIN): pins.internal_gpio_output_pin_schema,
            cv.Required(CONF_RESET_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_DATA_RATE_MHZ, default=40): cv.int_range(min=1, max=80),
        }
    )
    .extend(cv.polling_component_schema("never"))
)


def CONFIG_SCHEMA(config):
    """Validate the fixed SPD2010 display and publish metadata for LVGL.

    This must happen during config validation, before LVGL's final validation
    resolves byte order and draw rounding from display metadata.
    """
    config = _BASE_SCHEMA(config)
    display.add_metadata(
        config[CONF_ID],
        width=412,
        height=412,
        byte_order="big_endian",
        draw_rounding=4,
    )
    return config


async def to_code(config):
    add_idf_component(name="espressif/esp_lcd_spd2010", ref="2.0.0~1")
    var = cg.new_Pvariable(config[CONF_ID])
    await display.register_display(var, config)

    cg.add(var.set_clock_pin(await cg.gpio_pin_expression(config[CONF_CLOCK_PIN])))
    data_pins = [await cg.gpio_pin_expression(p) for p in config[CONF_DATA_PINS]]
    cg.add(var.set_data_pins(*data_pins))
    cg.add(var.set_cs_pin(await cg.gpio_pin_expression(config[CONF_CS_PIN])))
    cg.add(var.set_reset_pin(await cg.gpio_pin_expression(config[CONF_RESET_PIN])))
    cg.add(var.set_data_rate_hz(config[CONF_DATA_RATE_MHZ] * 1000000))

