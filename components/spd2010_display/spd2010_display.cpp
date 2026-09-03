#include "spd2010_display.h"

#include "esphome/core/log.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace spd2010_display {

static const char *const TAG = "spd2010_display";

bool IRAM_ATTR SPD2010Display::on_color_trans_done_(
    esp_lcd_panel_io_handle_t, esp_lcd_panel_io_event_data_t *, void *user_ctx) {
  auto *self = static_cast<SPD2010Display *>(user_ctx);
  if (self == nullptr || self->waiting_task_ == nullptr)
    return false;

  BaseType_t high_task_woken = pdFALSE;
  vTaskNotifyGiveFromISR(self->waiting_task_, &high_task_woken);
  return high_task_woken == pdTRUE;
}

bool SPD2010Display::wait_for_transfer_() {
  // draw_bitmap queues the colour DMA transaction. Do not let LVGL reuse its
  // draw buffer until that DMA transaction has actually completed.
  if (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1000)) == 0) {
    ESP_LOGE(TAG, "Timed out waiting for QSPI colour transfer");
    return false;
  }
  return true;
}

void SPD2010Display::setup() {
  if (!this->clock_pin_ || !this->data_pins_[0] || !this->data_pins_[1] ||
      !this->data_pins_[2] || !this->data_pins_[3] || !this->cs_pin_ ||
      !this->reset_pin_) {
    ESP_LOGE(TAG, "Missing QSPI/reset pins");
    this->mark_failed();
    return;
  }

  // SPI2 is dedicated to the panel; esp_lcd owns the bus directly.
  spi_bus_config_t bus_cfg{};
  bus_cfg.sclk_io_num = this->clock_pin_->get_pin();
  bus_cfg.data0_io_num = this->data_pins_[0]->get_pin();
  bus_cfg.data1_io_num = this->data_pins_[1]->get_pin();
  bus_cfg.data2_io_num = this->data_pins_[2]->get_pin();
  bus_cfg.data3_io_num = this->data_pins_[3]->get_pin();
  bus_cfg.max_transfer_sz = 412 * 412 * 2;

  esp_err_t err = spi_bus_initialize(SPI2_HOST, &bus_cfg, SPI_DMA_CH_AUTO);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "spi_bus_initialize failed: %s", esp_err_to_name(err));
    this->mark_failed();
    return;
  }

  esp_lcd_panel_io_spi_config_t io_cfg{};
  io_cfg.cs_gpio_num = this->cs_pin_->get_pin();
  io_cfg.dc_gpio_num = -1;
  io_cfg.spi_mode = 3;
  io_cfg.pclk_hz = this->data_rate_hz_;
  io_cfg.trans_queue_depth = 1;
  io_cfg.on_color_trans_done = &SPD2010Display::on_color_trans_done_;
  io_cfg.user_ctx = this;
  io_cfg.lcd_cmd_bits = 32;
  io_cfg.lcd_param_bits = 8;
  io_cfg.flags.quad_mode = 1;

  err = esp_lcd_new_panel_io_spi(
      SPI2_HOST, &io_cfg, &this->io_);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_lcd_new_panel_io_spi failed: %s", esp_err_to_name(err));
    this->mark_failed();
    return;
  }

  spd2010_vendor_config_t vendor_cfg{};
  vendor_cfg.flags.use_qspi_interface = 1;

  esp_lcd_panel_dev_config_t panel_cfg{};
  // Reset is behind PCA9554, so esp_lcd must not attempt a native GPIO reset.
  panel_cfg.reset_gpio_num = -1;
  panel_cfg.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
  panel_cfg.bits_per_pixel = 16;
  panel_cfg.vendor_config = &vendor_cfg;

  err = esp_lcd_new_panel_spd2010(this->io_, &panel_cfg, &this->panel_);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_lcd_new_panel_spd2010 failed: %s", esp_err_to_name(err));
    this->mark_failed();
    return;
  }

  // LCD reset is on PCA9554 EXIO2; GPIOPin keeps the expander transparent.
  this->reset_pin_->setup();
  this->reset_pin_->digital_write(false);
  delay(50);
  this->reset_pin_->digital_write(true);
  delay(120);

  // Do NOT call esp_lcd_panel_reset(): reset_gpio_num=-1 makes that fall back
  // to a software reset. We have already performed the board's real reset.
  err = esp_lcd_panel_init(this->panel_);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_lcd_panel_init failed: %s", esp_err_to_name(err));
    this->mark_failed();
    return;
  }

  err = esp_lcd_panel_disp_on_off(this->panel_, true);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_lcd_panel_disp_on_off failed: %s", esp_err_to_name(err));
    this->mark_failed();
    return;
  }

  this->setup_complete_ = true;
  ESP_LOGI(TAG, "Native SPD2010 QSPI display ready");
}

void SPD2010Display::draw_pixel_at(int, int, Color) {
  // This driver is deliberately a direct-blit LVGL display: the SPD2010's
  // four-pixel X alignment makes isolated framebuffer-less pixel writes unsafe
  // because they would overwrite three neighbouring pixels. LVGL uses the
  // bulk draw_pixels_at() path below.
  if (!this->warned_slow_path_) {
    ESP_LOGW(TAG, "Single-pixel drawing is unsupported; use LVGL/direct blits");
    this->warned_slow_path_ = true;
  }
}

void SPD2010Display::draw_pixels_at(
    int x_start, int y_start, int w, int h, const uint8_t *ptr,
    display::ColorOrder order, display::ColorBitness bitness, bool big_endian,
    int x_offset, int y_offset, int x_pad) {
  if (!this->setup_complete_ || !this->panel_ || !ptr || w <= 0 || h <= 0)
    return;

  // display.add_metadata() tells LVGL to provide precisely this format and to
  // round x regions to four pixels. Refuse anything else rather than silently
  // corrupt neighbouring pixels or RGB565 byte order.
  if (order != display::ColorOrder::COLOR_ORDER_RGB || bitness != display::ColorBitness::COLOR_BITNESS_565 ||
      !big_endian || x_offset != 0 || y_offset != 0 || x_pad != 0) {
    ESP_LOGE(TAG, "Unsupported blit format (expected packed RGB565 big-endian)");
    return;
  }

  const int x_end = x_start + w;
  const int y_end = y_start + h;
  if ((x_start & 3) != 0 || (x_end & 3) != 0) {
    ESP_LOGE(TAG, "Unaligned SPD2010 blit x=%d..%d; expected 4-pixel boundaries",
             x_start, x_end);
    return;
  }

  this->waiting_task_ = xTaskGetCurrentTaskHandle();
  // Clear any stale notification before queueing this transfer.
  ulTaskNotifyTake(pdTRUE, 0);

  esp_err_t err = esp_lcd_panel_draw_bitmap(
      this->panel_, x_start, y_start, x_end, y_end, ptr);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_lcd_panel_draw_bitmap failed: %s", esp_err_to_name(err));
    this->waiting_task_ = nullptr;
    return;
  }

  this->wait_for_transfer_();
  this->waiting_task_ = nullptr;
}

void SPD2010Display::dump_config() {
  ESP_LOGCONFIG(TAG, "SPD2010 native QSPI display:");
  ESP_LOGCONFIG(TAG, "  Size: 412x412");
  ESP_LOGCONFIG(TAG, "  SPI host: SPI2");
  ESP_LOGCONFIG(TAG, "  Data rate: %.1f MHz", this->data_rate_hz_ / 1000000.0f);
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
}

}  // namespace spd2010_display
}  // namespace esphome
