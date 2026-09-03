import esphome.codegen as cg # type: ignore
from esphome.components import select
import esphome.config_validation as cv
from .. import (
    CONF_PRANA_ID,
    PRANA_BLE_CLIENT_SCHEMA,
    prana_ble_ns,
    register_prana_child,
)

DEPENDENCIES = ["prana_ble"]
CODEOWNERS = ["@voed"]

CONF_DISPLAY_MODE = "display_mode"
CONF_FAN_MODE = "fan_mode"
CONF_DIRECT_CONTROL = "direct_control"
CONF_MODES = "modes"

# Display pages in the order the device cycles through them. The value is the PranaDisplayMode bit in
# prana_ble_const.h (slot 8 is unused).
DISPLAY_MODES = {
    "Fan": 0,
    "Temp inside": 1,
    "Temp outside": 2,
    "CO2": 3,
    "VOC": 4,
    "Humidity": 5,
    "Air quality": 6,
    "Pressure": 7,
    "Date": 9,
    "Time": 10,
}


def validate_modes(value):
    value = cv.ensure_list(cv.one_of(*DISPLAY_MODES))(value)
    if not value:
        raise cv.Invalid("modes must list at least one display mode")
    if len(set(value)) != len(value):
        raise cv.Invalid("modes must not repeat a display mode")
    return value

ICON_DISPLAY = "mdi:unfold-more-vertical"
ICON_FAN_AUTO="mdi:fan-auto"

PranaBLEDisplaySelect = prana_ble_ns.class_("PranaBLEDisplaySelect", select.Select, cg.Component)
PranaBLEFanSelect = prana_ble_ns.class_("PranaBLEFanSelect", select.Select, cg.Component)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.Optional(CONF_DISPLAY_MODE): select.select_schema(
                PranaBLEDisplaySelect,
                icon=ICON_DISPLAY
            ).extend(
                {
                    # Set the mode with a single command (0x5A + mode) instead of stepping left/right.
                    # Verified against the device; falls back to stepping if not confirmed.
                    cv.Optional(CONF_DIRECT_CONTROL, default=False): cv.boolean,
                    # Offer only these modes. Without the list they are detected from the device's
                    # sensors, stored, and applied on the next boot.
                    cv.Optional(CONF_MODES): validate_modes,
                }
            ),
            cv.Optional(CONF_FAN_MODE): select.select_schema(
                PranaBLEFanSelect,
                icon=ICON_FAN_AUTO
            ),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(PRANA_BLE_CLIENT_SCHEMA)
)

async def to_code(config):
    if CONF_DISPLAY_MODE in config:
        conf = config[CONF_DISPLAY_MODE]
        modes = conf.get(CONF_MODES, list(DISPLAY_MODES))
        sel = await select.new_select(conf, options=modes)
        await cg.register_component(sel, config)
        await register_prana_child(sel, config)
        if CONF_MODES in conf:
            mask = 0
            for mode in modes:
                mask |= 1 << DISPLAY_MODES[mode]
            cg.add(sel.set_configured_modes(mask))
        hub = await cg.get_variable(config[CONF_PRANA_ID])
        cg.add(hub.set_display_direct(conf[CONF_DIRECT_CONTROL]))

    if CONF_FAN_MODE in config:
        sel = await select.new_select(
            config[CONF_FAN_MODE],
            options=["Manual", "Auto", "Auto+"]
        )
        await cg.register_component(sel, config)
        await register_prana_child(sel, config)

