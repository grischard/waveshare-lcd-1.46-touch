#include "spd2010_touchscreen.h"

#include "esphome/core/log.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace esphome {
namespace spd2010_touch {

static const char *const TAG = "spd2010_touch";

esp_err_t SPD2010Touchscreen::i2c_error_to_esp_(i2c::ErrorCode err) {
  switch (err) {
    case i2c::ERROR_OK: return ESP_OK;
    case i2c::ERROR_INVALID_ARGUMENT: return ESP_ERR_INVALID_ARG;
    case i2c::ERROR_NOT_ACKNOWLEDGED: return ESP_ERR_NOT_FOUND;
    case i2c::ERROR_TIMEOUT: return ESP_ERR_TIMEOUT;
    case i2c::ERROR_NOT_INITIALIZED: return ESP_ERR_INVALID_STATE;
    case i2c::ERROR_TOO_LARGE: return ESP_ERR_INVALID_SIZE;
    default: return ESP_FAIL;
  }
}

esp_err_t SPD2010Touchscreen::raw_io_tx_param_(
    esp_lcd_panel_io_t *io, int, const void *param, size_t param_size) {
  auto *raw = reinterpret_cast<RawI2CPanelIO *>(io);
  if (!raw || !raw->parent || !raw->parent->i2c_bus_ || !param || param_size == 0)
    return ESP_ERR_INVALID_ARG;

  return i2c_error_to_esp_(raw->parent->i2c_bus_->write(
      SPD2010_ADDR, static_cast<const uint8_t *>(param), param_size));
}

esp_err_t SPD2010Touchscreen::raw_io_rx_param_(
    esp_lcd_panel_io_t *io, int, void *param, size_t param_size) {
  auto *raw = reinterpret_cast<RawI2CPanelIO *>(io);
  if (!raw || !raw->parent || !raw->parent->i2c_bus_ || !param || param_size == 0)
    return ESP_ERR_INVALID_ARG;

  return i2c_error_to_esp_(raw->parent->i2c_bus_->read(
      SPD2010_ADDR, static_cast<uint8_t *>(param), param_size));
}

esp_err_t SPD2010Touchscreen::raw_io_tx_color_(
    esp_lcd_panel_io_t *io, int cmd, const void *color, size_t color_size) {
  return raw_io_tx_param_(io, cmd, color, color_size);
}

esp_err_t SPD2010Touchscreen::raw_io_del_(esp_lcd_panel_io_t *) {
  return ESP_OK;
}

esp_err_t SPD2010Touchscreen::raw_io_register_event_callbacks_(
    esp_lcd_panel_io_t *, const esp_lcd_panel_io_callbacks_t *, void *) {
  return ESP_ERR_NOT_SUPPORTED;
}

void SPD2010Touchscreen::setup() {
  if (!this->i2c_bus_ || !this->interrupt_pin_) {
    ESP_LOGE(TAG, "I2C bus or interrupt pin not configured");
    this->mark_failed();
    return;
  }

  // Probe via ESPHome's bus abstraction.
  auto probe = this->i2c_bus_->write(SPD2010_ADDR, nullptr, 0);
  if (probe != i2c::ERROR_OK) {
    ESP_LOGE(TAG, "SPD2010 probe failed at 0x%02X: I2C error %d",
             SPD2010_ADDR, static_cast<int>(probe));
    this->mark_failed();
    return;
  }

  this->raw_io_.parent = this;
  this->raw_io_.base.rx_param = &SPD2010Touchscreen::raw_io_rx_param_;
  this->raw_io_.base.tx_param = &SPD2010Touchscreen::raw_io_tx_param_;
  this->raw_io_.base.tx_color = &SPD2010Touchscreen::raw_io_tx_color_;
  this->raw_io_.base.del = &SPD2010Touchscreen::raw_io_del_;
  this->raw_io_.base.register_event_callbacks =
      &SPD2010Touchscreen::raw_io_register_event_callbacks_;

  esp_lcd_touch_config_t cfg{};
  cfg.x_max = 412;
  cfg.y_max = 412;
  cfg.rst_gpio_num = GPIO_NUM_NC;
  cfg.int_gpio_num = GPIO_NUM_NC;
  cfg.levels.reset = 0;
  cfg.levels.interrupt = 0;
  cfg.flags.swap_xy = 0;
  cfg.flags.mirror_x = 0;
  cfg.flags.mirror_y = 0;

  esp_err_t err = esp_lcd_touch_new_i2c_spd2010(
      reinterpret_cast<esp_lcd_panel_io_handle_t>(&this->raw_io_.base),
      &cfg, &this->tp_);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_lcd_touch_new_i2c_spd2010 failed: %s", esp_err_to_name(err));
    this->mark_failed();
    return;
  }

  // Let Espressif settle the controller into point mode before IRQ delivery.
  for (int n = 0; n < 5; n++) {
    err = esp_lcd_touch_read_data(this->tp_);
    if (err != ESP_OK) {
      ESP_LOGW(TAG, "Warm-up read %d failed: %s", n + 1, esp_err_to_name(err));
    } else {
      esp_lcd_touch_point_data_t point[1]{};
      uint8_t count = 0;
      esp_lcd_touch_get_data(this->tp_, point, &count, 1);
    }
    vTaskDelay(pdMS_TO_TICKS(50));
  }

  this->interrupt_pin_->setup();
  this->attach_interrupt_(this->interrupt_pin_, gpio::INTERRUPT_FALLING_EDGE);

  ESP_LOGI(TAG, "SPD2010 touchscreen ready");
}

void SPD2010Touchscreen::update_touches() {
  if (!this->tp_) {
    this->skip_update_ = true;
    return;
  }

  esp_err_t err = esp_lcd_touch_read_data(this->tp_);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "esp_lcd_touch_read_data failed: %s", esp_err_to_name(err));
    this->skip_update_ = true;
    return;
  }

  esp_lcd_touch_point_data_t points[CONFIG_ESP_LCD_TOUCH_MAX_POINTS]{};
  uint8_t count = 0;
  err = esp_lcd_touch_get_data(
      this->tp_, points, &count, CONFIG_ESP_LCD_TOUCH_MAX_POINTS);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "esp_lcd_touch_get_data failed: %s", esp_err_to_name(err));
    this->skip_update_ = true;
    return;
  }

  for (uint8_t i = 0; i < count; i++) {
    // SPD2010 emits touch-up with strength == 0. Omitting that point lets
    // ESPHome transition the existing touch to RELEASED.
    if (points[i].strength == 0)
      continue;

    this->add_raw_touch_position_(
        i,
        static_cast<int16_t>(points[i].x),
        static_cast<int16_t>(points[i].y),
        static_cast<int16_t>(points[i].strength));
  }
}

void SPD2010Touchscreen::dump_config() {
  ESP_LOGCONFIG(TAG, "SPD2010 native touchscreen:");
  ESP_LOGCONFIG(TAG, "  I2C address: 0x%02X", SPD2010_ADDR);
  LOG_PIN("  Interrupt Pin: ", this->interrupt_pin_);
}

}  // namespace spd2010_touch
}  // namespace esphome
