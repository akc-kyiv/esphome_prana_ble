#include "prana_ble_sensors.h"
#include "esphome/core/log.h"

#ifdef USE_ESP32

namespace esphome {
namespace prana_ble {


void PranaBLESensors::dump_config() { ESP_LOGCONFIG("", "Prana BLE Sensors"); }
std::string PranaBLESensors::describe() { return "Prana BLE Sensors"; }


bool is_temp_valid(float temp)
{
  //TODO: test this
  if(abs(temp) > 60.0)
    return false;

  return true;
}

float convert_temp(short temp)
{
  return (byteswap(temp) & 0x3fff) / 10.0;
}

// 16-bit sensor fields are big-endian with bit 15 set when the sensor is installed/valid;
// absent sensors read as 0x0000.
static bool is_sensor_present(short raw)
{
  return (byteswap((uint16_t) raw) & 0x8000) != 0;
}

static void publish_nan(sensor::Sensor *s)
{
  if(s != nullptr)
    s->publish_state(NAN);
}

void PranaBLESensors::on_prana_state(bool state) {
  if(!state)
  {
    publish_nan(this->voltage_);
    publish_nan(this->temp_inside_out_);
    publish_nan(this->temp_inside_in_);
    publish_nan(this->temp_outside_out_);
    publish_nan(this->temp_outside_in_);
    publish_nan(this->humidity_);
    publish_nan(this->pressure_);
    publish_nan(this->tvoc_);
    publish_nan(this->co2_);
    publish_nan(this->frequency_);
    publish_nan(this->timestamp_);
  }
}
void PranaBLESensors::on_status(const PranaStatusPacket *data) {
  if(data == nullptr)
    return;
  
  float temp = 0.0;
  if(this->temp_inside_out_ != nullptr && is_sensor_present(data->temp_inside_out))
  {
    temp = convert_temp(data->temp_inside_out);
    if(is_temp_valid(temp))
      this->temp_inside_out_->publish_state(temp);
  }

  if(this->temp_inside_in_ != nullptr && is_sensor_present(data->temp_inside_in))
  {
    temp = convert_temp(data->temp_inside_in);
    if(is_temp_valid(temp))
      this->temp_inside_in_->publish_state(temp);
  }

  if(this->temp_outside_out_ != nullptr && is_sensor_present(data->temp_outside_out))
  {
    temp = convert_temp(data->temp_outside_out);
    if(is_temp_valid(temp))
      this->temp_outside_out_->publish_state(temp);
  }

  if(this->temp_outside_in_ != nullptr && is_sensor_present(data->temp_outside_in))
  {
    temp = convert_temp(data->temp_outside_in);
    if(is_temp_valid(temp))
      this->temp_outside_in_->publish_state(temp);
  }

  if(this->humidity_ != nullptr && (data->humidity & 0x80))
  {
    auto humidity = data->humidity & 0x7f;
    if(humidity <= 100)
      this->humidity_->publish_state(humidity);
  }

  // pressure is a 16-bit big-endian field (bytes 77-78) in mmHg
  if(this->pressure_ != nullptr && is_sensor_present(data->pressure))
    this->pressure_->publish_state(byteswap(data->pressure) & 0x3fff);

  if(this->tvoc_ != nullptr && is_sensor_present(data->voc))
    this->tvoc_->publish_state(byteswap(data->voc) & 0x7fff);

  if(this->co2_ != nullptr && is_sensor_present(data->co2))
    this->co2_->publish_state(byteswap(data->co2) & 0x3fff);

  if(this->voltage_ != nullptr && data->voltage > 0)
    this->voltage_->publish_state(data->voltage);

  if(this->frequency_ != nullptr && data->frequency > 0)
    this->frequency_->publish_state(data->frequency);

  // big-endian 32-bit seconds value from the status header. Unix time on some devices; on others it
  // is not affected by setting the clock and does not match the time shown on the display.
  if(this->timestamp_ != nullptr && data->timestamp > 0)
    this->timestamp_->publish_state(byteswap(data->timestamp));
}


}  // namespace prana_ble
}  // namespace esphome

#endif
