# SCS9009 serial bus servo

> ⚠️ **Work In Progress**
>
> This component is under active development. Only the position, feedback and torque commands are
> implemented.
>
> Compatible with ESPHome 2026.11 and newer: it only uses API that is in the 2026.11 release, and is
> built against both 2026.11.0 and the development tree. Check that before switching to a newer API.

`scs9009` implements the [FTServo](../ftservo/README.md) platform for Feetech SCS9009 serial
bus servos. It provides the UART bus, the FT-SCS frame format and the SCS/SCSCL memory table; the
platform layer in `ftservo` provides the model description, the motion state machine and the
entities.

## Bus

One bus entry manages the UART and all requests on it. Requests from all servos are queued and sent
one at a time.

```yaml
uart:
  id: servo_uart
  tx_pin: GPIO6
  rx_pin: GPIO7
  baud_rate: 1000000
  data_bits: 8
  parity: NONE
  stop_bits: 1

scs9009:
  id: servo_bus
  uart_id: servo_uart
```

- **uart_id** (*Optional*, [ID](https://esphome.io/guides/configuration-types#id)): The UART bus.
  Required when more than one `uart` is configured.
- **timeout** (*Optional*, [Time](https://esphome.io/guides/configuration-types#config-time)): How
  long to wait for a response frame. Defaults to `10ms`.
- **min_interval** (*Optional*, [Time](https://esphome.io/guides/configuration-types#config-time)):
  Minimum silence between two frames. Defaults to `2ms`; the servos need a short gap between
  commands.

The bus uses 8N1 and needs half-duplex wiring. The baud rate is set on the `uart` component; SCS
servos ship at 1 Mbaud or 500 kbaud.

## Servo

```yaml
ftservo:
  - platform: scs9009
    id: pan
    scs9009_id: servo_bus
    address: 1
    range: 300
    zero_position: 500
```

- **address** (*Required*, int): Servo ID on the bus, `0` to `253`. `254` is the broadcast address
  and cannot be used for a servo.
- **scs9009_id** (*Required*, [ID](https://esphome.io/guides/configuration-types#id)): The bus.
- All options from the [FTServo platform](../ftservo/README.md#servo-model-options), including
  `range`, `zero_position`, `inverted`, the position limits, the default speed and time and the
  automations.

At startup the servo reads registers 2 to 8 once. The `END` register selects the byte order and the
response level decides whether writes wait for an acknowledgement. Until that read completes the
byte order is big endian and writes are fire-and-forget, which is the safe pair for these servos.
If the read fails, a warning is logged and polling continues.

## Group writes and sync reads

The bus implements the two broadcast instructions from the FT-SCS protocol:

- **SYNC WRITE** (`0x83`): one frame writes the same register block of up to eight servos, so all
  of them start together. It is used by [`ftservo_group`](../ftservo_group/README.md) and needs no
  response.
- **SYNC READ** (`0x82`): one frame asks up to eight servos for the same register block. Each servo
  answers in request order with its own status and checksum; the bus dispatches each response to
  the matching servo.

The C++ interface, for a component that drives several servos:

```cpp
bus->begin_sync_write();
bus->add_sync_write(&shoulder, 1, SCS_REG_GOAL_POSITION, shoulder_data, 6);
bus->add_sync_write(&elbow, 2, SCS_REG_GOAL_POSITION, elbow_data, 6);
bus->end_sync_write();
```

Every entry of one SYNC WRITE must use the same register and the same data length; the frame is
sent on the next bus loop. `abort_sync_write()` throws a half-collected batch away. The sync read
takes the servos in request order:

```cpp
SCS9009Servo *servos[] = {&shoulder, &elbow};
bus->queue_sync_read(servos, 2, SCS_REG_PRESENT_POSITION, SCS_FEEDBACK_LENGTH);
```

A SYNC WRITE is a broadcast, so the servos do not acknowledge it; a missing servo is only visible
through the next feedback read. A SYNC READ fails the servos that did not answer within `timeout`
and reports the error through their `on_error` trigger.

## Memory map used

| Register | Address | Description |
|---|---|---|
| `END` | 2 | `1` = big endian |
| Response level | 8 | `0` = only reads and ping answer |
| Torque enable | 40 | `0` off, `1` on |
| Goal position | 42 | Two bytes, followed by time and speed |
| Goal time | 44 | Two bytes, milliseconds; takes precedence over speed |
| Goal speed | 46 | Two bytes, steps/s |
| Present position | 56 | Two bytes |
| Present speed | 58 | Two bytes, signed |
| Present load | 60 | Two bytes, 0.1 %, signed |
| Present voltage | 62 | One byte, 0.1 V |
| Present temperature | 63 | One byte, °C |
| Async write flag | 64 | One byte, set while a REG WRITE waits for ACTION |
| Servo status | 65 | One byte, voltage/temperature/overload bits |
| Present moving | 66 | One byte |

A feedback read fetches registers 56 to 66 in one request. The status bits of the response and of
register 65 are combined and mapped to the FTServo error mask.

## Feedback

`sensor` exposes position in steps, speed in steps/s, load in percent, voltage in volts, temperature
in °C and a model-defined current. SCS9009 does not document a current register, so the `current`
sensor stays unknown unless a future model enables it.

## Notes

- One request is on the bus at a time. A request takes at least one loop iteration, so with several
  servos on one bus the feedback cycle is `update_interval` plus the transfer time of each servo.
  A SYNC WRITE or SYNC READ covers all of its servos in that one request.
- A frame that is identical to the last transmitted frame is treated as a UART echo and ignored.
- A response is accepted only when its address and length match the request and its checksum is
  correct. A bad frame fails the request; the next poll tries again.
- A missing servo does not stop the bus or the other servos. The servo logs the failure through its
  `on_error` trigger and the component warning.
- Not implemented yet: register writes for EEPROM settings (ID, baud rate, limits), PWM/motor mode,
  and calibration commands.
