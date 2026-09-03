#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/i2c/i2c_bus.h"
#include "esphome/components/touchscreen/touchscreen.h"

#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_io_interface.h"
#include "esp_lcd_touch.h"
#include "esp_lcd_touch_spd2010.h"

namespace esphome {
namespace spd2010_touch {

class SPD2010Touchscreen : public touchscreen::Touchscreen {
 public:
  void set_i2c_bus(i2c::I2CBus *bus) { this->i2c_bus_ = bus; }
  void set_interrupt_pin(InternalGPIOPin *pin) { this->interrupt_pin_ = pin; }

  void setup() override;
  void dump_config() override;

 protected:
  struct RawI2CPanelIO {
    esp_lcd_panel_io_t base{};
    SPD2010Touchscreen *parent{nullptr};
  };

  static esp_err_t raw_io_rx_param_(esp_lcd_panel_io_t *, int, void *, size_t);
  static esp_err_t raw_io_tx_param_(esp_lcd_panel_io_t *, int, const void *, size_t);
  static esp_err_t raw_io_tx_color_(esp_lcd_panel_io_t *, int, const void *, size_t);
  static esp_err_t raw_io_del_(esp_lcd_panel_io_t *);
  static esp_err_t raw_io_register_event_callbacks_(esp_lcd_panel_io_t *,
                                                     const esp_lcd_panel_io_callbacks_t *,
                                                     void *);
  static esp_err_t i2c_error_to_esp_(i2c::ErrorCode err);

  void update_touches() override;

  static constexpr uint8_t SPD2010_ADDR = 0x53;
  i2c::I2CBus *i2c_bus_{nullptr};
  InternalGPIOPin *interrupt_pin_{nullptr};
  RawI2CPanelIO raw_io_{};
  esp_lcd_touch_handle_t tp_{nullptr};
};

}  // namespace spd2010_touch
}  // namespace esphome
