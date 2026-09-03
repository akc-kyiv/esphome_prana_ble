#include <type_traits>

#include "esphome/core/log.h"
#include "prana_ble_display_select.h"

namespace esphome {
namespace prana_ble {

// salt for the preference key; change when the stored layout changes
static const uint32_t DISPLAY_MODES_PREF_VERSION = 0x50444D01;

// Fill the select's option container from a mode set. Written against the container type so that it
// compiles with SelectTraits holding FixedVector<const char *> (current ESPHome) as well as
// std::vector<std::string> (older releases).
template<typename Options> static void fill_display_options(Options &options, uint16_t modes) {
  if constexpr (!std::is_same<Options, std::vector<std::string>>::value) {
    int count = 0;
    for (uint8_t i = 0; i < PranaDisplayMode::MODE_COUNT; i++)
      if (modes & (1u << i))
        count++;
    options.init(count);
  }
  for (uint8_t i = 0; i < PranaDisplayMode::MODE_COUNT; i++)
    if (modes & (1u << i))
      options.push_back(PRANA_DISPLAY_MODES[i].c_str());  // static storage, valid for the life of the program
}

PranaDisplayMode get_display_mode_from_string(const std::string& mode_string)
{
  for(int i=0; i < PRANA_DISPLAY_MODES.size(); i++)
  {
    if(PRANA_DISPLAY_MODES[i] == mode_string)
      return static_cast<PranaDisplayMode>(i);
  }
  return PranaDisplayMode::FAN;
}

/* Which modes to offer
 *
 * Home Assistant reads a select's option list only when it connects to the ESP, so the list has to be
 * final at boot. It either comes from the config (`modes:`), or it is detected: the pages follow the
 * sensors the device reports (prana_display_modes_from_status), the result is stored in flash and the
 * list is built from the stored value on the next boot. Detection only ever adds modes (a page that was
 * shown once stays), so a wrong list is corrected with `modes:`. Either way the hub is told the set and
 * refuses modes outside it at once instead of hunting for them on the device.
 */
void PranaBLEDisplaySelect::setup() {
  if(this->configured_modes_ != 0)
  {
    // the options were generated from `modes:`; the hub only needs to know them
    this->parent_->set_display_supported(this->configured_modes_);
    return;
  }
  this->pref_ = global_preferences->make_preference<uint16_t>(this->get_object_id_hash() ^ DISPLAY_MODES_PREF_VERSION);
  uint16_t stored = 0;
  if(this->pref_.load(&stored) && (stored & PRANA_DISPLAY_MODES_BASE) == PRANA_DISPLAY_MODES_BASE &&
     (stored & ~PRANA_DISPLAY_MODES_ALL) == 0)
  {
    this->detected_modes_ = stored;
    this->apply_modes_(stored);
  }
}

void PranaBLEDisplaySelect::apply_modes_(uint16_t modes) {
  std::decay_t<decltype(this->traits.get_options())> options;
  fill_display_options(options, modes);
  this->traits.set_options(options);
  this->parent_->set_display_supported(modes);
}

void PranaBLEDisplaySelect::detect_modes_(const PranaStatusPacket *data) {
  uint16_t modes = prana_display_modes_from_status(data) | this->detected_modes_;
  if(modes == this->detected_modes_)
    return;
  bool first = this->detected_modes_ == 0;
  this->detected_modes_ = modes;
  this->pref_.save(&modes);
  this->parent_->set_display_supported(modes);
  ESP_LOGI(TAG, "[%s] Display modes on this device: %s (%s; restart the ESP to update the option list in Home Assistant)",
           this->get_name().c_str(), prana_display_modes_to_string(modes).c_str(), first ? "stored" : "updated");
}

void PranaBLEDisplaySelect::control(const std::string &value) {
  if (!this->parent_->is_connected()) {
    ESP_LOGW(TAG, "Not connected, cannot handle control call yet.");
    return;
  }
  if (!this->parent_->has_status()) {
    ESP_LOGW(TAG, "No status from device yet, cannot handle control call yet.");
    return;
  }
  short ival = get_display_mode_from_string(value);
  ESP_LOGD(TAG, "Setting display mode: %s (%i)", value.c_str(), ival);
  // No optimistic publish: the state follows what the device reports (on_status).
  parent_->set_display_mode(ival);
}


void PranaBLEDisplaySelect::on_status(const PranaStatusPacket *data) {
  if(data == nullptr)
    return;
  if(this->configured_modes_ == 0)
    this->detect_modes_(data);
  if(this->parent_->display_busy())
    return;  // hold the state while a change is in progress; the final mode is published when it ends
  if(data->display_mode >= PRANA_DISPLAY_MODES.size())
  {
    ESP_LOGE(TAG, "Incorrect display mode: %i", data->display_mode);
    return;
  }
  const std::string &option = PRANA_DISPLAY_MODES[data->display_mode];
  if(!this->has_option(option))
  {
    // slot 8 ("unused"), or a page left out of `modes:`: the select cannot show it. Warn once per page,
    // not on every 1 Hz status.
    if(this->unlisted_mode_ != data->display_mode)
    {
      this->unlisted_mode_ = data->display_mode;
      ESP_LOGW(TAG, "[%s] Device shows display mode %i (%s), which is not one of the select's options",
               this->get_name().c_str(), data->display_mode, option.c_str());
    }
    return;
  }
  this->unlisted_mode_ = 0xFF;
  this->publish_state(option);
}
void PranaBLEDisplaySelect::dump_config() {
  LOG_SELECT("", describe().c_str(), this);
  if(this->configured_modes_ != 0)
    ESP_LOGCONFIG(TAG, "  Modes (from config): %s", prana_display_modes_to_string(this->configured_modes_).c_str());
  else if(this->detected_modes_ != 0)
    ESP_LOGCONFIG(TAG, "  Modes (detected): %s", prana_display_modes_to_string(this->detected_modes_).c_str());
  else
    ESP_LOGCONFIG(TAG, "  Modes: all (not detected yet)");
}
std::string PranaBLEDisplaySelect::describe() { return "Prana BLE Select Display"; }
}  // namespace prana_ble
}  // namespace esphome