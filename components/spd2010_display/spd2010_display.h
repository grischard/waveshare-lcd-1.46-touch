#pragma once

#include "esphome/components/display/display.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"

#include "driver/spi_master.h"
#include "esp_lcd_io_spi.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_spd2010.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace esphome {
namespace spd2010_display {

class SPD2010Display : public display::Display {
 public:
  void set_clock_pin(InternalGPIOPin *pin) { this->clock_pin_ = pin; }
  void set_data_pins(InternalGPIOPin *d0, InternalGPIOPin *d1,
                     InternalGPIOPin *d2, InternalGPIOPin *d3) {
    this->data_pins_[0] = d0;
    this->data_pins_[1] = d1;
    this->data_pins_[2] = d2;
    this->data_pins_[3] = d3;
  }
  void set_cs_pin(InternalGPIOPin *pin) { this->cs_pin_ = pin; }
  void set_reset_pin(GPIOPin *pin) { this->reset_pin_ = pin; }
  void set_data_rate_hz(uint32_t hz) { this->data_rate_hz_ = hz; }

  void setup() override;
  void update() override {}
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }

  display::DisplayType get_display_type() override {
    return display::DISPLAY_TYPE_COLOR;
  }

  void draw_pixel_at(int x, int y, Color color) override;
  void draw_pixels_at(int x_start, int y_start, int w, int h,
                      const uint8_t *ptr, display::ColorOrder order,
                      display::ColorBitness bitness, bool big_endian,
                      int x_offset, int y_offset, int x_pad) override;

 protected:
  int get_width_internal() override { return 412; }
  int get_height_internal() override { return 412; }

  static bool on_color_trans_done_(esp_lcd_panel_io_handle_t io,
                                   esp_lcd_panel_io_event_data_t *edata,
                                   void *user_ctx);
  bool wait_for_transfer_();

  InternalGPIOPin *clock_pin_{nullptr};
  InternalGPIOPin *data_pins_[4]{nullptr, nullptr, nullptr, nullptr};
  InternalGPIOPin *cs_pin_{nullptr};
  GPIOPin *reset_pin_{nullptr};
  uint32_t data_rate_hz_{40000000};

  esp_lcd_panel_io_handle_t io_{nullptr};
  esp_lcd_panel_handle_t panel_{nullptr};
  TaskHandle_t waiting_task_{nullptr};
  bool setup_complete_{false};
  bool warned_slow_path_{false};
};

}  // namespace spd2010_display
}  // namespace esphome
