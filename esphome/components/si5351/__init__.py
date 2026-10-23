"""Si5351 I2C programmable clock generator.

The YAML describes the *frequency plan*; every register value is derived at
code generation time by :mod:`esphome.components.si5351.si5351_plan`, so the
device itself only has to write a pre-computed table (see AN619).
"""

import logging
import re

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation, pins
from esphome.components import i2c
from esphome.const import CONF_ADDRESS, CONF_CHANNEL, CONF_FREQUENCY, CONF_ID

from . import si5351_plan
from .si5351_plan import PlanError

_LOGGER = logging.getLogger(__name__)

CODEOWNERS = ["@Jasionf", "@parker-int64"]
DEPENDENCIES = ["i2c"]
MULTI_CONF = True
DOMAIN = "si5351"

si5351_ns = cg.esphome_ns.namespace("si5351")
Si5351Component = si5351_ns.class_("Si5351Component", cg.Component, i2c.I2CDevice)
SetFrequencyAction = si5351_ns.class_("SetFrequencyAction", automation.Action)

CONF_SI5351_ID = "si5351_id"
CONF_VARIANT = "variant"
CONF_PACKAGE = "package"
CONF_CRYSTAL = "crystal"
CONF_CLKIN = "clkin"
CONF_PLL_A = "pll_a"
CONF_PLL_B = "pll_b"
CONF_CLOCKS = "clocks"
CONF_CLK = "pin"
CONF_PLL = "pll"
CONF_SOURCE = "source"
CONF_VCO = "vco"
CONF_INTEGER_MODE = "integer_mode"
CONF_SPREAD_SPECTRUM = "spread_spectrum"
CONF_VCXO = "vcxo"
CONF_PROFILE = "profile"
CONF_AMOUNT = "amount"
CONF_PULL_RANGE = "pull_range"
CONF_LOAD_CAPACITANCE = "load_capacitance"
CONF_DRIVE_STRENGTH = "drive_strength"
CONF_INVERTED = "inverted"
CONF_R_DIVIDER = "r_divider"
CONF_DISABLED_STATE = "disabled_state"
CONF_PHASE_OFFSET = "phase_offset"
CONF_OEB_CONTROLLED = "oeb_controlled"
CONF_POWERED = "powered"
CONF_OEB_PIN = "oeb_pin"
CONF_SSEN_PIN = "ssen_pin"
CONF_INTR_PIN = "intr_pin"
CONF_RAW_REGISTERS = "raw_registers"

#: variant: (has CLKIN, has VCXO, has SSEN pin, has INTR pin, has A0 pin)
VARIANTS = {
    "a": (False, False, True, False, True),
    "b": (False, True, True, False, False),
    "c": (True, False, False, True, False),
}
#: package: (clock outputs, has A0 pin, has OEB pin, available variants)
PACKAGES = {
    "qfn20": (8, True, True, ("a", "b", "c")),
    "qfn16": (4, True, True, ("a", "b", "c")),
    "msop10": (3, False, False, ("a",)),
}
DISABLED_STATES = ("low", "high", "high_z", "never")
LOAD_CAPACITANCES = {6: "6pF", 8: "8pF", 10: "10pF"}

_TIME_UNITS = {"ns": 1e-9, "us": 1e-6, "ms": 1e-3, "s": 1.0}


def validate_clock_pin(value):
    if isinstance(value, int):
        if 0 <= value <= 7:
            return value
        raise cv.Invalid("clock pin must be CLK0..CLK7")
    match = re.fullmatch(r"CLK([0-7])", str(value).strip().upper())
    if match is None:
        raise cv.Invalid(f"'{value}' is not a valid clock output, use CLK0..CLK7")
    return int(match.group(1))


def _validate_capacitance(value):
    match = re.fullmatch(r"\s*([0-9]+(?:\.[0-9]+)?)\s*p[Ff]\s*", str(value))
    if match is None:
        raise cv.Invalid(f"'{value}' is not a valid capacitance, use e.g. 10pF")
    picofarads = float(match.group(1))
    if picofarads not in LOAD_CAPACITANCES:
        raise cv.Invalid(
            "load_capacitance must be one of "
            + ", ".join(LOAD_CAPACITANCES.values())
        )
    return int(picofarads)


def _validate_drive_strength(value):
    match = re.fullmatch(r"\s*([0-9]+)\s*mA\s*", str(value))
    if match is not None:
        value = int(match.group(1))
    try:
        value = cv.int_(value)
    except cv.Invalid as err:
        raise cv.Invalid("drive_strength must be one of 2mA, 4mA, 6mA or 8mA") from err
    if value not in si5351_plan.DRIVE_BITS:
        raise cv.Invalid("drive_strength must be one of 2mA, 4mA, 6mA or 8mA")
    return value


def _validate_r_divider(value):
    try:
        value = cv.int_(value)
    except cv.Invalid as err:
        raise cv.Invalid("r_divider must be a power of two between 1 and 128") from err
    if value not in si5351_plan.R_DIVIDERS:
        raise cv.Invalid("r_divider must be a power of two between 1 and 128")
    return value


def _validate_pull_range(value):
    match = re.fullmatch(
        r"\s*([0-9]+(?:\.[0-9]+)?)\s*(ppm)?\s*", str(value), re.IGNORECASE
    )
    if match is None:
        raise cv.Invalid(f"'{value}' is not a valid pull range, use e.g. 30ppm")
    value = float(match.group(1))
    if not 0 < value <= 1000:
        raise cv.Invalid("pull_range must be between 1 and 1000 ppm")
    return value


def _validate_phase_offset(value):
    match = re.fullmatch(r"\s*([0-9]+(?:\.[0-9]+)?)\s*(ns|us|ms|s)\s*", str(value))
    if match is None:
        raise cv.Invalid(f"'{value}' is not a valid time, use e.g. 2ns or 0.5us")
    return float(match.group(1)) * _TIME_UNITS[match.group(2)]


def _validate_integer_mode(value):
    # YAML turns bare true/false into booleans, so accept both spellings.
    if isinstance(value, bool):
        return "true" if value else "false"
    return cv.one_of("auto", "true", "false", lower=True)(value)


def _validate_raw_registers(value):
    if not isinstance(value, list):
        raise cv.Invalid("raw_registers must be a list of [address, value] pairs")
    result = []
    for entry in value:
        if not isinstance(entry, list) or len(entry) != 2:
            raise cv.Invalid("each raw_registers entry must be [address, value]")
        result.append(
            (
                cv.int_range(min=0, max=255)(entry[0]),
                cv.int_range(min=0, max=255)(entry[1]),
            )
        )
    return result


CRYSTAL_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_FREQUENCY, default="27MHz"): cv.All(
            cv.frequency, cv.one_of(25e6, 27e6)
        ),
        cv.Optional(CONF_LOAD_CAPACITANCE, default="10pF"): _validate_capacitance,
    }
)

CLKIN_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_FREQUENCY): cv.All(cv.frequency, cv.Range(min=10e6, max=100e6)),
    }
)

SPREAD_SPECTRUM_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_PROFILE, default="down"): cv.one_of(
            "down", "center", lower=True
        ),
        cv.Required(CONF_AMOUNT): cv.All(
            cv.percentage, cv.Range(min=0.0, max=0.025)
        ),
    }
)

VCXO_SCHEMA = cv.Schema({cv.Required(CONF_PULL_RANGE): _validate_pull_range})

PLL_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_SOURCE, default="xtal"): cv.one_of("xtal", "clkin", lower=True),
        cv.Optional(CONF_VCO): cv.All(cv.frequency, cv.Range(min=600e6, max=900e6)),
        cv.Optional(CONF_INTEGER_MODE, default="auto"): _validate_integer_mode,
        cv.Optional(CONF_SPREAD_SPECTRUM): SPREAD_SPECTRUM_SCHEMA,
        cv.Optional(CONF_VCXO): VCXO_SCHEMA,
    }
)

CLOCK_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_CLK): validate_clock_pin,
        cv.Optional(CONF_FREQUENCY): cv.All(
            cv.frequency,
            cv.Range(min=si5351_plan.OUTPUT_MIN_HZ, max=si5351_plan.OUTPUT_MAX_HZ),
        ),
        cv.Optional(CONF_SOURCE, default="multisynth"): cv.one_of(
            "multisynth", "xtal", "clkin", lower=True
        ),
        cv.Optional(CONF_PLL): cv.one_of("a", "b", lower=True),
        cv.Optional(CONF_INTEGER_MODE, default="auto"): _validate_integer_mode,
        cv.Optional(CONF_R_DIVIDER): _validate_r_divider,
        cv.Optional(CONF_DRIVE_STRENGTH, default="2mA"): _validate_drive_strength,
        cv.Optional(CONF_INVERTED, default=False): cv.boolean,
        cv.Optional(CONF_DISABLED_STATE, default="low"): cv.one_of(
            *DISABLED_STATES, lower=True
        ),
        cv.Optional(CONF_PHASE_OFFSET, default="0ns"): _validate_phase_offset,
        cv.Optional(CONF_OEB_CONTROLLED, default=True): cv.boolean,
        cv.Optional(CONF_POWERED, default=True): cv.boolean,
    }
)

#: Plan used when ``clocks`` is omitted. It reproduces the original hard coded
#: behaviour of this component: a 27 MHz crystal with CLK1 at 6.144 MHz.
LEGACY_CLOCK = CLOCK_SCHEMA({CONF_CLK: 1, CONF_FREQUENCY: 6.144e6})


def _clocks(config):
    return config.get(CONF_CLOCKS) or [LEGACY_CLOCK]


def _clkin_divider(frequency):
    for divider in (1, 2, 4, 8):
        if 10e6 <= frequency / divider <= 40e6:
            return divider
    raise cv.Invalid(
        f"CLKIN at {frequency / 1e6:g} MHz cannot be divided into the 10..40 MHz "
        "PLL reference range"
    )


def _validate_device(config):
    variant = config[CONF_VARIANT]
    package = config[CONF_PACKAGE]
    has_clkin, has_vcxo, has_ssen, has_intr, has_a0_variant = VARIANTS[variant]
    _, has_a0_package, has_oeb, variants = PACKAGES[package]
    has_address = has_a0_variant and has_a0_package

    if variant not in variants:
        raise cv.Invalid(
            f"the {package} package is only available in the "
            + " and ".join(f"Si5351{v.upper()}" for v in variants)
        )
    if CONF_OEB_PIN in config and not has_oeb:
        raise cv.Invalid(f"the {package} package has no OEB pin")
    if CONF_CLKIN in config and not has_clkin:
        raise cv.Invalid(f"clkin is only available on the Si5351C, not on Si5351{variant.upper()}")
    if CONF_INTR_PIN in config and (not has_intr or package != "qfn20"):
        raise cv.Invalid("intr_pin is only available on the Si5351C in the 20-QFN package")
    if CONF_SSEN_PIN in config and (not has_ssen or package != "qfn20"):
        raise cv.Invalid("ssen_pin is only available on the Si5351A/B in the 20-QFN package")

    for name in (CONF_PLL_A, CONF_PLL_B):
        pll = config.get(name, {})
        if pll.get(CONF_SOURCE, "xtal") == "clkin" and not has_clkin:
            raise cv.Invalid(f"{name}.source 'clkin' is only available on the Si5351C")
        if CONF_VCXO in pll:
            if not has_vcxo:
                raise cv.Invalid(f"{name}.vcxo is only available on the Si5351B")
            if name != CONF_PLL_B:
                raise cv.Invalid("the VCXO can only use pll_b")
        if CONF_SPREAD_SPECTRUM in pll and name != CONF_PLL_A:
            raise cv.Invalid("spread spectrum is only available on pll_a")

    if not has_address and config[CONF_ADDRESS] != 0x60:
        raise cv.Invalid(
            f"Si5351{variant.upper()} in the {package} package has no A0 pin, "
            "the I2C address is fixed at 0x60"
        )
    if config[CONF_ADDRESS] not in (0x60, 0x61):
        raise cv.Invalid("the Si5351 I2C address must be 0x60 or 0x61 (A0 pin)")
    return config


def _validate_clocks(config):
    package = config[CONF_PACKAGE]
    limit = PACKAGES[package][0]
    has_clkin = VARIANTS[config[CONF_VARIANT]][0]

    for clock in _clocks(config):
        if clock[CONF_CLK] >= limit:
            raise cv.Invalid(
                f"CLK{clock[CONF_CLK]} is not available in the {package} package, "
                f"only CLK0..CLK{limit - 1} exist"
            )
        if clock[CONF_SOURCE] == "clkin" and not has_clkin:
            raise cv.Invalid("CLKIN bypass is only available on the Si5351C")
        if clock[CONF_SOURCE] != "multisynth" and CONF_FREQUENCY in clock:
            raise cv.Invalid(
                f"CLK{clock[CONF_CLK]} bypasses the synthesis stages, so its frequency "
                "is determined by the reference and must not be set"
            )
        if clock[CONF_SOURCE] == "multisynth" and CONF_FREQUENCY not in clock:
            raise cv.Invalid(
                f"CLK{clock[CONF_CLK]} needs a frequency when it is driven by the "
                "multisynth"
            )
    _build_plan(config)
    return config


def _build_plan(config):
    """Turn a validated config into a (plan, register pairs) tuple."""
    crystal = config[CONF_CRYSTAL]
    xtal_hz = crystal[CONF_FREQUENCY]

    clkin_hz = None
    clkin_div = 1
    if CONF_CLKIN in config:
        clkin_hz = config[CONF_CLKIN][CONF_FREQUENCY]
        clkin_div = _clkin_divider(clkin_hz)

    plls = []
    for index, name in enumerate((CONF_PLL_A, CONF_PLL_B)):
        raw = config.get(name, {})
        spread = raw.get(CONF_SPREAD_SPECTRUM)
        if spread is not None:
            spread = {
                "profile": spread[CONF_PROFILE],
                "amount": spread[CONF_AMOUNT],
                "pin_controlled": CONF_SSEN_PIN in config,
            }
        vcxo = raw.get(CONF_VCXO)
        if vcxo is not None:
            vcxo = {"pull_range_ppm": vcxo[CONF_PULL_RANGE]}
        plls.append(
            si5351_plan.PllRequest(
                index=index,
                source=raw.get(CONF_SOURCE, "xtal"),
                vco=raw.get(CONF_VCO),
                integer_mode=raw.get(CONF_INTEGER_MODE, "auto"),
                spread_spectrum=spread,
                vcxo=vcxo,
            )
        )

    clocks = []
    for clock in _clocks(config):
        source = clock[CONF_SOURCE]
        if source == "multisynth":
            frequency = clock[CONF_FREQUENCY]
        elif source == "clkin":
            frequency = clkin_hz
        else:
            frequency = xtal_hz
        pll = clock.get(CONF_PLL)
        clocks.append(
            si5351_plan.ClockRequest(
                index=clock[CONF_CLK],
                frequency=frequency,
                pll=None if pll is None else ("a", "b").index(pll),
                source=source,
                integer_mode=clock[CONF_INTEGER_MODE],
                r_divider=clock.get(CONF_R_DIVIDER),
                drive_strength=clock[CONF_DRIVE_STRENGTH],
                inverted=clock[CONF_INVERTED],
                disabled_state=clock[CONF_DISABLED_STATE],
                oeb_controlled=clock[CONF_OEB_CONTROLLED],
                powered=clock[CONF_POWERED],
                phase_offset=clock[CONF_PHASE_OFFSET],
            )
        )

    try:
        plan = si5351_plan.solve(
            xtal_hz=xtal_hz,
            plls=plls,
            clocks=clocks,
            clkin_hz=clkin_hz,
            clkin_div=clkin_div,
            interrupt_mask=0x00 if CONF_INTR_PIN in config else 0x1F,
            crystal_load_capacitance=crystal[CONF_LOAD_CAPACITANCE],
            clock_count=PACKAGES[config[CONF_PACKAGE]][0],
        )
    except PlanError as err:
        raise cv.Invalid(str(err)) from err

    registers = dict(plan.registers)
    if clkin_hz is not None:
        encoded = {1: 0b00, 2: 0b01, 4: 0b10, 8: 0b11}[clkin_div]
        registers[si5351_plan.REG_PLL_SOURCE] = (
            registers[si5351_plan.REG_PLL_SOURCE] & 0x3F
        ) | (encoded << 6)
    registers.update(dict(config.get(CONF_RAW_REGISTERS, [])))
    return plan, sorted(registers.items())


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(Si5351Component),
            cv.Optional(CONF_VARIANT, default="a"): cv.one_of(*VARIANTS, lower=True),
            cv.Optional(CONF_PACKAGE, default="qfn20"): cv.one_of(*PACKAGES, lower=True),
            cv.Optional(CONF_CRYSTAL, default={}): CRYSTAL_SCHEMA,
            cv.Optional(CONF_CLKIN): CLKIN_SCHEMA,
            cv.Optional(CONF_PLL_A, default={}): PLL_SCHEMA,
            cv.Optional(CONF_PLL_B, default={}): PLL_SCHEMA,
            cv.Optional(CONF_CLOCKS): cv.All(
                cv.ensure_list(CLOCK_SCHEMA), cv.Length(min=1)
            ),
            cv.Optional(CONF_OEB_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_SSEN_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_INTR_PIN): pins.internal_gpio_input_pin_schema,
            cv.Optional(CONF_RAW_REGISTERS): _validate_raw_registers,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(i2c.i2c_device_schema(0x60)),
    _validate_device,
    _validate_clocks,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    if not config.get(CONF_CLOCKS):
        _LOGGER.warning(
            "si5351: no clocks configured, falling back to the legacy plan "
            "(27 MHz crystal, CLK1 at 6.144 MHz). Please add a 'clocks:' list."
        )

    plan, registers = _build_plan(config)

    name = f"SI5351_REGS_{config[CONF_ID].id.upper()}"
    payload = ", ".join(
        f"0x{register:02X}, 0x{value:02X}" for register, value in registers
    )
    cg.add_global(cg.RawStatement(f"static const uint8_t {name}[] = {{{payload}}};"))
    cg.add(var.set_register_plan(cg.RawExpression(name), len(registers)))

    for clock in plan.clocks:
        cg.add(
            var.add_clock(
                clock.index,
                int(round(clock.achieved)),
                int(round(plan.pll(clock.pll).realised_vco)),
                clock.request.source == "multisynth",
            )
        )

    if CONF_OEB_PIN in config:
        pin = await cg.gpio_pin_expression(config[CONF_OEB_PIN])
        cg.add(var.set_oeb_pin(pin))
    if CONF_SSEN_PIN in config:
        pin = await cg.gpio_pin_expression(config[CONF_SSEN_PIN])
        cg.add(var.set_ssen_pin(pin))

    for note in plan.notes:
        _LOGGER.info("si5351(%s): %s", config[CONF_ID], note)


SI5351_SET_FREQUENCY_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(Si5351Component),
        cv.Required(CONF_CHANNEL): validate_clock_pin,
        cv.Required(CONF_FREQUENCY): cv.templatable(
            cv.All(
                cv.frequency,
                cv.Range(min=si5351_plan.OUTPUT_MIN_HZ, max=si5351_plan.OUTPUT_MAX_HZ),
            )
        ),
    }
)


@automation.register_action(
    "si5351.set_frequency",
    SetFrequencyAction,
    SI5351_SET_FREQUENCY_SCHEMA,
    synchronous=True,
)
async def si5351_set_frequency_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent)
    cg.add(var.set_channel(config[CONF_CHANNEL]))
    frequency = await cg.templatable(config[CONF_FREQUENCY], args, cg.double)
    cg.add(var.set_frequency(frequency))
    return var
