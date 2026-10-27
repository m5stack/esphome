from esphome.components import output

from .. import m5_chain_base_ns

M5ChainLedOutput = m5_chain_base_ns.class_("M5ChainLedOutput", output.FloatOutput)
LedChannel = m5_chain_base_ns.enum("LedChannel", is_class=True)

CHANNEL_MAP = {
    "led_brightness": LedChannel.LED_BRIGHTNESS,
    "rgb_brightness": LedChannel.RGB_BRIGHTNESS,
    "rgb_red": LedChannel.RGB_RED,
    "rgb_green": LedChannel.RGB_GREEN,
    "rgb_blue": LedChannel.RGB_BLUE,
}
