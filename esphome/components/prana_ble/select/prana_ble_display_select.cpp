#include "esphome/core/log.h"
#include "prana_ble_display_select.h"

namespace esphome {
namespace prana_ble {


PranaDisplayMode get_display_mode_from_string(const std::string& mode_string)
{
  for(int i=0; i < PRANA_DISPLAY_MODES.size(); i++)
  {
    if(PRANA_DISPLAY_MODES[i] == mode_string)
      return static_cast<PranaDisplayMode>(i);
  }
  return PranaDisplayMode::FAN;
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
    // e.g. slot 8 ("unused"): the select would reject a value that is not one of its options
    ESP_LOGW(TAG, "Device reports display mode %i (%s), which is not a configured option",
             data->display_mode, option.c_str());
    return;
  }
  this->publish_state(option);
}
void PranaBLEDisplaySelect::dump_config() {
  LOG_SELECT("", describe().c_str(), this);
}
std::string PranaBLEDisplaySelect::describe() { return "Prana BLE Select Display"; }
}  // namespace prana_ble
}  // namespace esphome