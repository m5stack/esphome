"""Si5351 frequency plan solver.

Implements the register calculations from Skyworks AN619 "Manually Generating
an Si5351 Register Map for 10-MSOP and 20-QFN Devices" so a complete register
map can be derived from user facing output frequencies at code generation time.

Everything in here is pure Python and runs on the host; the device only ever
writes pre-computed register values.
"""

from __future__ import annotations

import math
from dataclasses import dataclass, field

# --- Synthesis stage limits (AN619 sections 2 and 3) ------------------------
VCO_MIN_HZ = 600_000_000.0
VCO_MAX_HZ = 900_000_000.0
FEEDBACK_MIN = 15.0
FEEDBACK_MAX = 90.0
MULTISYNTH_MIN = 8.0
MULTISYNTH_MAX = 2048.0
#: Largest valid P3 denominator (2 ** 20 - 1).
MULTISYNTH_DENOM = 1_048_575
R_DIVIDERS = (1, 2, 4, 8, 16, 32, 64, 128)
#: Multisynth 6/7 are integer only and accept even values in this range.
MS67_MIN = 6
MS67_MAX = 254
#: Multisynth divider values that are valid below the fractional range.
MULTISYNTH_SPECIAL = (4, 6, 8)
#: At most two distinct output frequencies may exceed this value.
HIGH_FREQ_LIMIT_HZ = 112_500_000.0
OUTPUT_MIN_HZ = 2_500.0
OUTPUT_MAX_HZ = 200_000_000.0
SSC_MODULATION_HZ = 31_500.0
#: VCXO feedback multisynth denominator, fixed by AN619 section 7.
VCXO_DENOM = 106
#: Reported clock frequency must match the request within this relative error.
FREQUENCY_TOLERANCE = 1e-5

MSN_BASE = 26
MSN_STRIDE = 8
MS_BASE = 42
MS_STRIDE = 8

REG_INTERRUPT_MASK = 2
REG_OUTPUT_ENABLE = 3
REG_OEB_MASK = 9
REG_PLL_SOURCE = 15
REG_CLOCK_CONTROL = 16
REG_DISABLE_STATE_0_3 = 24
REG_DISABLE_STATE_4_7 = 25
REG_MS6_P1 = 90
REG_MS7_P1 = 91
REG_R6_R7_DIV = 92
REG_SSC_EN = 149
REG_SSC_MODE = 151
REG_PHOFF = 165
REG_PLL_RESET = 177
REG_XTAL_LOAD = 183

XTAL_LOAD_BITS = {6: 0b01, 8: 0b10, 10: 0b11}
DISABLED_STATE_BITS = {"low": 0b00, "high": 0b01, "high_z": 0b10, "never": 0b11}
CLOCK_SOURCE_BITS = {"xtal": 0b00, "clkin": 0b01, "multisynth": 0b11}
DRIVE_BITS = {2: 0, 4: 1, 6: 2, 8: 3}


class PlanError(ValueError):
    """Raised when a requested frequency plan cannot be realised."""


@dataclass
class PllRequest:
    index: int
    source: str = "xtal"
    vco: float | None = None
    integer_mode: str = "auto"
    spread_spectrum: dict | None = None
    vcxo: dict | None = None


@dataclass
class PllPlan:
    index: int
    source: str
    f_pfd: float
    vco: float
    a: int
    b: int
    c: int
    integer_mode: bool
    spread_spectrum: dict | None = None
    vcxo: dict | None = None

    @property
    def ratio(self) -> float:
        return self.a + self.b / self.c

    @property
    def realised_vco(self) -> float:
        return self.f_pfd * self.ratio


@dataclass
class ClockRequest:
    index: int
    frequency: float
    pll: int | None = None
    source: str = "multisynth"
    integer_mode: str = "auto"
    r_divider: int | None = None
    drive_strength: int = 2
    inverted: bool = False
    disabled_state: str = "low"
    oeb_controlled: bool = True
    powered: bool = True
    phase_offset: float = 0.0


@dataclass
class ClockPlan:
    request: ClockRequest
    pll: int
    r_div: int
    divider: float
    a: int
    b: int
    c: int
    integer_mode: bool
    divby4: bool
    achieved: float
    phase_offset: int = 0

    @property
    def index(self) -> int:
        return self.request.index


@dataclass
class Plan:
    registers: list[tuple[int, int]]
    plls: list[PllPlan]
    clocks: list[ClockPlan]
    enable_mask: int
    oeb_mask: int
    notes: list[str] = field(default_factory=list)

    def clock(self, index: int) -> ClockPlan | None:
        for clock in self.clocks:
            if clock.index == index:
                return clock
        return None

    def pll(self, index: int) -> PllPlan:
        for pll in self.plls:
            if pll.index == index:
                return pll
        raise KeyError(index)


# --- Low level register encoding -------------------------------------------


def p1_p2_p3(a: int, b: int, c: int) -> tuple[int, int, int]:
    """Encode a + b/c into the P1/P2/P3 representation of AN619."""
    floor_term = (128 * b) // c
    p1 = 128 * a + floor_term - 512
    p2 = 128 * b - c * floor_term
    return p1, p2, c


def _snap(divider: float) -> float:
    """Round a divider that is within VCO quantisation noise of an integer.

    The VCO can only be synthesised on a ``f_pfd / 2**20`` grid, so an intended
    exact integer divider can come out a few ppm off. Snapping keeps those cases
    in integer mode instead of falling into fractional mode with b = P3 - 1.
    """
    nearest = round(divider)
    if abs(divider - nearest) <= 1e-6 * nearest:
        return float(nearest)
    return divider


def _fractional_params(divider: float, allow_integer: bool) -> tuple[int, int, int, bool]:
    """Split a divider into a + b/c, using integer mode for even integers."""
    divider = _snap(divider)
    a = int(math.floor(divider))
    b = int(round((divider - a) * MULTISYNTH_DENOM))
    if b >= MULTISYNTH_DENOM:
        a += 1
        b = 0
    if b == 0:
        if allow_integer and a % 2 == 0:
            return a, 0, 1, True
        # Odd integers have to stay in fractional mode; c == 1 means integer mode.
        return a, 0, MULTISYNTH_DENOM, False
    return a, b, MULTISYNTH_DENOM, False


def _feedback_params(
    ratio: float, vcxo: bool, allow_integer: bool
) -> tuple[int, int, int, bool]:
    if vcxo:
        a = int(math.floor(ratio))
        b = int(round((ratio - a) * VCXO_DENOM))
        if b >= VCXO_DENOM:
            a += 1
            b = 0
        return a, b, VCXO_DENOM, False
    return _fractional_params(ratio, allow_integer)


def _r_encode(r_div: int) -> int:
    return R_DIVIDERS.index(r_div)


def _output_multisynth_valid(divider: float, high_freq_output: bool) -> bool:
    if high_freq_output:
        return (
            abs(divider - round(divider)) < 1e-9
            and int(round(divider)) % 2 == 0
            and MS67_MIN <= divider <= MS67_MAX
        )
    if abs(divider - round(divider)) < 1e-9 and int(round(divider)) in MULTISYNTH_SPECIAL:
        return True
    return MULTISYNTH_MIN + 1.0 / MULTISYNTH_DENOM <= divider <= MULTISYNTH_MAX


# --- Search ----------------------------------------------------------------


def _exact_vco_candidates(request: ClockRequest, budget: int = 200) -> set[float]:
    """VCO values that divide into ``request.frequency`` with b == 0.

    Only products of the form ``frequency * divider * r_div`` are considered,
    which keeps the candidate count bounded even for very low frequencies.
    """
    high_freq_output = request.index >= 6
    low = VCO_MIN_HZ / request.frequency
    high = VCO_MAX_HZ / request.frequency
    r_options = (request.r_divider,) if request.r_divider is not None else R_DIVIDERS
    candidates: set[float] = set()
    for r_div in r_options:
        first = max(1, int(math.ceil(low / r_div)))
        last = int(math.floor(high / r_div))
        if high_freq_output:
            first = max(first, MS67_MIN)
            last = min(last, MS67_MAX)
        if last < first:
            continue
        # Sub-sample when the window is huge so pathological requests stay fast.
        stride = max(1, (last - first + 1) // max(1, budget // len(r_options)))
        for divider in range(first, last + 1, stride):
            if high_freq_output:
                if divider % 2:
                    continue
            elif divider < 9 and divider not in MULTISYNTH_SPECIAL:
                continue
            vco = request.frequency * divider * r_div
            if VCO_MIN_HZ <= vco <= VCO_MAX_HZ:
                candidates.add(vco)
    return candidates


def _vco_candidates(group: list[ClockRequest], forced_vco: float | None) -> list[float]:
    if forced_vco is not None:
        return [forced_vco]
    candidates: set[float] = set()
    # Anchor on the highest frequencies: they have the fewest exact solutions,
    # and every other clock can follow them with a fractional divider.
    for request in sorted(group, key=lambda item: -item.frequency)[:3]:
        candidates |= _exact_vco_candidates(request)
    # A coarse grid keeps fractional solutions reachable for every other case.
    span = VCO_MAX_HZ - VCO_MIN_HZ
    for step in range(49):
        candidates.add(VCO_MIN_HZ + span * step / 48.0)
    return list(candidates)


def _best_output_divider(
    realised_vco: float, request: ClockRequest
) -> tuple[int, float, int, int, int, bool, bool] | None:
    """Pick the R divider and the multisynth parameters for a single clock."""
    high_freq_output = request.index >= 6
    r_options = (request.r_divider,) if request.r_divider is not None else R_DIVIDERS
    best = None
    for r_div in r_options:
        divider = _snap(realised_vco / (request.frequency * r_div))
        if not _output_multisynth_valid(divider, high_freq_output):
            continue
        if request.integer_mode == "true":
            if abs(divider - round(divider)) > 1e-9:
                continue
            a, b, c, integer = int(round(divider)), 0, 1, True
        elif request.integer_mode == "false":
            a, b, c, integer = _fractional_params(divider, allow_integer=False)
        else:
            allow_integer = request.phase_offset == 0
            a, b, c, integer = _fractional_params(divider, allow_integer)
        divby4 = abs(divider - 4.0) < 1e-9 and not high_freq_output
        achieved = realised_vco / (a + b / c) / r_div
        error = abs(achieved - request.frequency) / request.frequency
        if error > FREQUENCY_TOLERANCE:
            continue
        score = (1 if b else 0, 0 if integer else 1, int(math.log2(r_div)), error)
        if best is None or score < best[0]:
            best = (score, (r_div, divider, a, b, c, integer, divby4))
    return None if best is None else best[1]


def _solve_group(
    group: list[ClockRequest],
    f_pfd: float,
    forced_vco: float | None,
    vcxo: dict | None,
    allow_integer_feedback: bool,
) -> tuple[tuple[int, int, int, bool], float, dict[int, tuple]] | None:
    best = None
    for vco in _vco_candidates(group, forced_vco):
        if not VCO_MIN_HZ <= vco <= VCO_MAX_HZ:
            continue
        ratio = vco / f_pfd
        if not FEEDBACK_MIN <= ratio <= FEEDBACK_MAX:
            continue
        a, b, c, integer = _feedback_params(
            ratio, vcxo is not None, allow_integer_feedback
        )
        realised_vco = f_pfd * (a + b / c)
        solutions = {}
        fractional = 0
        forced_fractional = 0
        r_penalty = 0
        for request in group:
            entry = _best_output_divider(realised_vco, request)
            if entry is None:
                solutions = None
                break
            solutions[request.index] = entry
            fractional += 1 if entry[3] else 0
            forced_fractional += 0 if entry[5] else 1
            r_penalty += int(math.log2(entry[0]))
        if solutions is None:
            continue
        score = (
            fractional,
            forced_fractional,
            r_penalty,
            0 if b == 0 else 1,
            -realised_vco,
        )
        if best is None or score < best[0]:
            best = (score, (a, b, c, integer), realised_vco, solutions)
    if best is None:
        return None
    return best[1], best[2], best[3]


def _ssc_registers(f_pfd: float, ratio: float, amplitude: float, profile: str) -> dict:
    ssudp = int(math.floor(f_pfd / (4.0 * SSC_MODULATION_HZ)))
    if ssudp < 1:
        raise PlanError(
            "spread spectrum needs a PLL reference of at least "
            f"{4 * SSC_MODULATION_HZ:.0f} Hz"
        )
    if profile == "center":
        ssup = 128.0 * ratio * amplitude / ((1.0 - amplitude) * ssudp)
        ssdn = 128.0 * ratio * amplitude / ((1.0 + amplitude) * ssudp)
        mode = 1
    else:
        ssup = 0.0
        ssdn = 64.0 * ratio * amplitude / ((1.0 + amplitude) * ssudp)
        mode = 0
    ssup_p1 = int(math.floor(ssup))
    ssdn_p1 = int(math.floor(ssdn))
    values = {
        "ssudp": ssudp,
        "ssdn_p1": ssdn_p1,
        "ssdn_p2": int(round(32767 * (ssdn - ssdn_p1))),
        "ssdn_p3": 32767,
        "ssup_p1": ssup_p1,
        "ssup_p2": int(round(32767 * (ssup - ssup_p1))) if mode else 0,
        "ssup_p3": 32767 if mode else 1,
        "mode": mode,
    }
    for key in ("ssdn_p2", "ssdn_p3", "ssup_p2", "ssup_p3", "ssdn_p1", "ssup_p1"):
        if not 0 <= values[key] <= 32767:
            raise PlanError(f"spread spectrum parameter {key} is out of range")
    if not 0 <= values["ssudp"] <= 0xFFF:
        raise PlanError("spread spectrum modulation parameter SSUDP is out of range")
    return values


# --- Public entry point -----------------------------------------------------


def solve(
    xtal_hz: float,
    plls: list[PllRequest],
    clocks: list[ClockRequest],
    clkin_hz: float | None = None,
    clkin_div: int = 1,
    interrupt_mask: int = 0x1F,
    crystal_load_capacitance: int = 10,
    clock_count: int = 8,
) -> Plan:
    """Derive a full register map for the given frequency plan."""
    if not clocks:
        raise PlanError("at least one clock must be configured")

    notes: list[str] = []

    indices = [request.index for request in clocks]
    if len(set(indices)) != len(indices):
        raise PlanError("each CLK pin may only be configured once")

    high_freq = {
        round(request.frequency)
        for request in clocks
        if request.frequency > HIGH_FREQ_LIMIT_HZ
    }
    if len(high_freq) > 2:
        raise PlanError(
            "at most two distinct output frequencies above "
            f"{HIGH_FREQ_LIMIT_HZ / 1e6:.1f} MHz can be generated at once"
        )

    for request in clocks:
        if not OUTPUT_MIN_HZ <= request.frequency <= OUTPUT_MAX_HZ:
            raise PlanError(
                f"CLK{request.index} frequency {request.frequency:g} Hz is outside the "
                f"{OUTPUT_MIN_HZ:g}..{OUTPUT_MAX_HZ:g} Hz range of the Si5351"
            )
        if request.index >= 6 and request.frequency > HIGH_FREQ_LIMIT_HZ:
            raise PlanError(
                f"CLK{request.index} cannot exceed {HIGH_FREQ_LIMIT_HZ / 1e6:.1f} MHz "
                "because Multisynth6/7 only divide by even integers"
            )
        if request.phase_offset:
            if request.index >= 6:
                raise PlanError(f"CLK{request.index} has no phase offset register")
            if request.frequency > HIGH_FREQ_LIMIT_HZ:
                raise PlanError(
                    f"CLK{request.index} phase offset needs a fractional multisynth "
                    "divider, so the frequency must stay at or below "
                    f"{HIGH_FREQ_LIMIT_HZ / 1e6:.1f} MHz"
                )

    pll_by_index = {request.index: request for request in plls}

    def _pfd_for(request: PllRequest) -> float:
        if request.source == "clkin":
            if clkin_hz is None:
                raise PlanError("PLL source 'clkin' requires a CLKIN frequency")
            return clkin_hz / clkin_div
        return xtal_hz

    # Assign clocks to PLLs. Explicit assignments always win; everything else
    # is packed onto PLLA first and split only when a single VCO cannot serve
    # all of the remaining clocks.
    groups: dict[int, list[ClockRequest]] = {0: [], 1: []}
    unassigned: list[ClockRequest] = []
    for request in clocks:
        if request.pll is None:
            unassigned.append(request)
        else:
            groups[request.pll].append(request)

    attempts: list[dict[int, list[ClockRequest]]] = []
    if unassigned:
        for target in (0, 1):
            candidate = {0: list(groups[0]), 1: list(groups[1])}
            candidate[target].extend(unassigned)
            attempts.append(candidate)
        for moved in unassigned:
            for target in (0, 1):
                candidate = {0: list(groups[0]), 1: list(groups[1])}
                candidate[target].append(moved)
                candidate[1 - target].extend(
                    request for request in unassigned if request is not moved
                )
                attempts.append(candidate)
    else:
        attempts.append({0: list(groups[0]), 1: list(groups[1])})

    last_error: PlanError | None = None
    pll_plans: dict[int, PllPlan] = {}
    clock_entries: dict[int, tuple[int, tuple]] = {}
    for attempt in attempts:
        trial_plls: dict[int, PllPlan] = {}
        trial_entries: dict[int, tuple[int, tuple]] = {}
        try:
            for pll_index in (0, 1):
                group = attempt[pll_index]
                if not group:
                    continue
                pll_request = pll_by_index[pll_index]
                f_pfd = _pfd_for(pll_request)
                allow_integer = (
                    pll_request.integer_mode != "false"
                    and pll_request.spread_spectrum is None
                )
                result = _solve_group(
                    group, f_pfd, pll_request.vco, pll_request.vcxo, allow_integer
                )
                if result is None:
                    raise PlanError(
                        f"no valid VCO for PLL {'AB'[pll_index]} with "
                        + ", ".join(
                            f"CLK{request.index}={request.frequency / 1e6:g} MHz"
                            for request in group
                        )
                    )
                (a, b, c, integer), vco, solutions = result
                if pll_request.integer_mode == "true" and not integer:
                    raise PlanError(
                        f"PLL {'AB'[pll_index]} feedback ratio {vco / f_pfd:.6f} is not "
                        "an even integer, integer_mode cannot be used"
                    )
                trial_plls[pll_index] = PllPlan(
                    index=pll_index,
                    source=pll_request.source,
                    f_pfd=f_pfd,
                    vco=vco,
                    a=a,
                    b=b,
                    c=c,
                    integer_mode=integer,
                    spread_spectrum=pll_request.spread_spectrum,
                    vcxo=pll_request.vcxo,
                )
                for request in group:
                    trial_entries[request.index] = (pll_index, solutions[request.index])
        except PlanError as err:
            last_error = err
            continue
        pll_plans = trial_plls
        clock_entries = trial_entries
        break
    else:
        raise last_error or PlanError("unable to find a valid frequency plan")

    clock_plans: list[ClockPlan] = []
    for request in clocks:
        pll_index, (r_div, divider, a, b, c, integer, divby4) = clock_entries[
            request.index
        ]
        pll = pll_plans[pll_index]
        achieved = pll.realised_vco / (a + b / c) / r_div
        phase_offset = 0
        if request.phase_offset:
            phase_offset = int(round(request.phase_offset * 4.0 * pll.realised_vco))
            if not 0 <= phase_offset <= 0x7F:
                raise PlanError(
                    f"CLK{request.index} phase offset "
                    f"{request.phase_offset * 1e9:.3f} ns does not fit into the 7 bit "
                    "phase offset register"
                )
        clock_plans.append(
            ClockPlan(
                request=request,
                pll=pll_index,
                r_div=r_div,
                divider=divider,
                a=a,
                b=b,
                c=c,
                integer_mode=integer,
                divby4=divby4,
                achieved=achieved,
                phase_offset=phase_offset,
            )
        )

    registers: dict[int, int] = {REG_INTERRUPT_MASK: interrupt_mask}

    # PLL input sources; CLKIN_DIV is OR'ed in by the caller.
    source_bits = 0
    for pll in pll_plans.values():
        if pll.source == "clkin":
            source_bits |= 1 << (2 + pll.index)
    registers[REG_PLL_SOURCE] = source_bits

    for pll in pll_plans.values():
        base = MSN_BASE + pll.index * MSN_STRIDE
        p1, p2, p3 = p1_p2_p3(pll.a, pll.b, pll.c)
        registers[base + 0] = (p3 >> 8) & 0xFF
        registers[base + 1] = p3 & 0xFF
        registers[base + 2] = (p1 >> 16) & 0x03
        registers[base + 3] = (p1 >> 8) & 0xFF
        registers[base + 4] = p1 & 0xFF
        registers[base + 5] = (((p3 >> 16) & 0x0F) << 4) | ((p2 >> 16) & 0x0F)
        registers[base + 6] = (p2 >> 8) & 0xFF
        registers[base + 7] = p2 & 0xFF

    enable_mask = 0xFF
    oeb_mask = 0
    disable_state_0_3 = 0
    disable_state_4_7 = 0
    for plan in clock_plans:
        request = plan.request
        control = 0
        if not request.powered:
            control |= 0x80
        if request.index < 6:
            if plan.integer_mode:
                control |= 0x40
        if plan.pll:
            control |= 0x20
        if request.inverted:
            control |= 0x10
        control |= (CLOCK_SOURCE_BITS[request.source] & 0x03) << 2
        control |= DRIVE_BITS[request.drive_strength] & 0x03
        registers[REG_CLOCK_CONTROL + request.index] = control

        if request.powered:
            enable_mask &= ~(1 << request.index) & 0xFF
        if not request.oeb_controlled:
            oeb_mask |= 1 << request.index

        state = DISABLED_STATE_BITS[request.disabled_state]
        if request.index < 4:
            disable_state_0_3 |= state << (2 * request.index)
        else:
            disable_state_4_7 |= state << (2 * (request.index - 4))

        base = MS_BASE + request.index * MS_STRIDE
        if request.source != "multisynth":
            notes.append(
                f"CLK{request.index} bypasses the synthesis stages and outputs the "
                f"{request.source} reference directly"
            )
            if request.index < 6:
                registers[base + 2] = 0  # R divider = 1, DIVBY4 off
            else:
                # CLK6/CLK7 keep their R divider in the shared register 92.
                shift = 4 if request.index == 7 else 0
                registers[REG_R6_R7_DIV] = registers.get(REG_R6_R7_DIV, 0) & ~(
                    0x07 << shift
                )
            continue

        if request.index >= 6:
            registers[REG_MS6_P1 if request.index == 6 else REG_MS7_P1] = int(
                round(plan.divider)
            )
            shift = 4 if request.index == 7 else 0
            registers[REG_R6_R7_DIV] = registers.get(REG_R6_R7_DIV, 0) | (
                (_r_encode(plan.r_div) & 0x07) << shift
            )
            continue

        p1, p2, p3 = p1_p2_p3(plan.a, plan.b, plan.c)
        registers[base + 0] = (p3 >> 8) & 0xFF
        registers[base + 1] = p3 & 0xFF
        registers[base + 2] = (
            ((_r_encode(plan.r_div) & 0x07) << 4)
            | ((0b11 << 2) if plan.divby4 else 0)
            | ((p1 >> 16) & 0x03)
        )
        registers[base + 3] = (p1 >> 8) & 0xFF
        registers[base + 4] = p1 & 0xFF
        registers[base + 5] = (((p3 >> 16) & 0x0F) << 4) | ((p2 >> 16) & 0x0F)
        registers[base + 6] = (p2 >> 8) & 0xFF
        registers[base + 7] = p2 & 0xFF

    # Outputs without a configured clock are powered down and left disabled;
    # their reset value would otherwise route the raw crystal out (CLKx_SRC=00).
    configured = {plan.index for plan in clock_plans}
    for index in range(clock_count):
        if index not in configured:
            registers[REG_CLOCK_CONTROL + index] = 0x80

    # Bit 6 of registers 22 and 23 is FBA_INT / FBB_INT, independent of which
    # clock actually uses Multisynth 6 or 7 (AN619 register 22/23).
    for pll in pll_plans.values():
        if pll.integer_mode:
            register = REG_CLOCK_CONTROL + 6 + pll.index
            registers[register] = registers.get(register, 0) | 0x40

    registers[REG_DISABLE_STATE_0_3] = disable_state_0_3
    registers[REG_DISABLE_STATE_4_7] = disable_state_4_7
    registers[REG_OUTPUT_ENABLE] = enable_mask
    registers[REG_OEB_MASK] = oeb_mask

    for pll in pll_plans.values():
        if pll.spread_spectrum is None:
            continue
        if pll.index != 0:
            raise PlanError("spread spectrum is only available on PLL A")
        amplitude = pll.spread_spectrum["amount"]
        ssc = _ssc_registers(
            pll.f_pfd, pll.ratio, amplitude, pll.spread_spectrum["profile"]
        )
        registers[REG_SSC_EN] = 0x80 | ((ssc["ssdn_p2"] >> 8) & 0x7F)
        registers[150] = ssc["ssdn_p2"] & 0xFF
        registers[REG_SSC_MODE] = (ssc["mode"] << 7) | ((ssc["ssdn_p3"] >> 8) & 0x7F)
        registers[152] = ssc["ssdn_p3"] & 0xFF
        registers[153] = ssc["ssdn_p1"] & 0xFF
        registers[154] = (((ssc["ssudp"] >> 8) & 0x0F) << 4) | (
            (ssc["ssdn_p1"] >> 8) & 0x0F
        )
        registers[155] = ssc["ssudp"] & 0xFF
        registers[156] = (ssc["ssup_p2"] >> 8) & 0x7F
        registers[157] = ssc["ssup_p2"] & 0xFF
        registers[158] = (ssc["ssup_p3"] >> 8) & 0x7F
        registers[159] = ssc["ssup_p3"] & 0xFF
        registers[160] = ssc["ssup_p1"] & 0xFF
        registers[161] = (ssc["ssup_p1"] >> 8) & 0x0F
        if pll.spread_spectrum.get("pin_controlled"):
            # SSC_EN is OR'ed with the SSEN pin, so the register bit has to stay
            # clear for the pin to be able to disable spread spectrum.
            registers[REG_SSC_EN] &= 0x7F
        notes.append(
            f"PLL A spread spectrum {pll.spread_spectrum['profile']} "
            f"{amplitude * 100:.2f}% at {SSC_MODULATION_HZ / 1e3:.1f} kHz"
        )

    for pll in pll_plans.values():
        if pll.vcxo is None:
            continue
        if pll.index != 1:
            raise PlanError("the VCXO is only available on PLL B")
        apr = pll.vcxo["pull_range_ppm"]
        value = int(round(1.03 * (128 * pll.a + pll.b * apr / 1e6)))
        if not 0 <= value <= 0x3FFFFF:
            raise PlanError("VCXO parameter is out of range")
        registers[162] = value & 0xFF
        registers[163] = (value >> 8) & 0xFF
        registers[164] = (value >> 16) & 0x3F
        notes.append(f"PLL B VCXO pull range +/-{apr:g} ppm")

    for plan in clock_plans:
        if plan.phase_offset:
            registers[REG_PHOFF + plan.index] = plan.phase_offset & 0x7F

    registers[REG_PLL_RESET] = 0xA0
    # Register 183: XTAL_CL[1:0] plus bits 5:0, which AN619 fixes to 0b010010.
    registers[REG_XTAL_LOAD] = (XTAL_LOAD_BITS[crystal_load_capacitance] << 6) | 0b010010

    return Plan(
        registers=sorted(registers.items()),
        plls=[pll_plans[index] for index in sorted(pll_plans)],
        clocks=clock_plans,
        enable_mask=enable_mask,
        oeb_mask=oeb_mask,
        notes=notes,
    )
