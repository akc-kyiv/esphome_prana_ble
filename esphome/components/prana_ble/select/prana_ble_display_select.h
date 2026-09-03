#pragma once

#include "esphome/components/select/select.h"
#include "esphome/core/component.h"
#include "esphome/core/optional.h"
#include "esphome/core/preferences.h"
#include "../prana_ble_child.h"
#include "../prana_ble_const.h"
#include "../prana_ble_hub.h"

namespace esphome {
namespace prana_ble {

class PranaBLEDisplaySelect : public select::Select, public PranaBLEClient, public Component {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

  /* PranaClient status update */
  void on_status(const PranaStatusPacket *data) override;
  void on_prana_state(bool is_ready) override{};
  std::string describe() override;

  /** The modes listed under `modes:` in the config, as a bit set (1 << PranaDisplayMode). 0 = detect them. */
  void set_configured_modes(uint16_t modes) { this->configured_modes_ = modes; }

 protected:
  void control(const std::string &value) override;
  void detect_modes_(const PranaStatusPacket *data);
  void apply_modes_(uint16_t modes);

  uint16_t configured_modes_{0};
  uint16_t detected_modes_{0};
  uint8_t unlisted_mode_{0xFF};  // page the device shows that is not an option; warned about once
  ESPPreferenceObject pref_;
};

}  // namespace prana_ble
}  // namespace esphome
