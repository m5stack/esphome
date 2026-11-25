# Stack-chan motion

> ⚠️ **Work In Progress**
>
> This component is under active development.
>
> Compatible with ESPHome 2026.11 and newer: it only uses API that is in the 2026.11 release, and is
> built against both 2026.11.0 and the development tree. Check that before switching to a newer API.

`stackchan_motion` turns two [FTServo](../ftservo/README.md) instances into a two axis head: a
**yaw** servo that turns the head left and right and a **pitch** servo that raises and lowers it.
Both goals are written together, so with a transport that supports grouped writes (for example
`scs9009` and `SYNC WRITE`) the axes start at the same moment.

It is deliberately small. It covers a pan/tilt head only:

- two axes, one servo each
- per axis limits in degrees, plus a home position
- non-blocking absolute, relative and direction based motion
- the kinematics of the head: angles and the look direction are convertible in both directions

It does not do inverse kinematics for an arm, trajectories with velocity and acceleration limits,
or collision checks. Those need a full kinematic chain and are a separate component.

## Configuration

```yaml
ftservo:
  - platform: scs9009
    id: head_yaw
    scs9009_id: servo_bus
    address: 1
    range: 300
    zero_position: 500
  - platform: scs9009
    id: head_pitch
    scs9009_id: servo_bus
    address: 2
    range: 300
    zero_position: 500

stackchan_motion:
  id: head
  yaw_servo: head_yaw
  pitch_servo: head_pitch
  min_yaw: -90
  max_yaw: 90
  min_pitch: -30
  max_pitch: 60
  home_yaw: 0
  home_pitch: 0
  nudge_step: 10
  default_time: 600
```

- **yaw_servo** / **pitch_servo** (*Required*, [ID](https://esphome.io/guides/configuration-types#id)):
  The two servos. They must be on the same bus and use the same servo model.
- **min_yaw** / **max_yaw** (*Optional*, float): Yaw limits in degrees. Defaults to `-90` and `90`.
- **min_pitch** / **max_pitch** (*Optional*, float): Pitch limits in degrees. Defaults to `-45` and
  `45`.
- **home_yaw** / **home_pitch** (*Optional*, float): Where `stackchan_motion.home` goes. Defaults to
  `0`. Must be inside the limits.
- **nudge_step** (*Optional*, float): Degrees one press of the direction buttons moves. Defaults to
  `10`.
- **default_speed** (*Optional*, int): Speed used by a move without its own speed, in steps/s.
  Defaults to `0`, which keeps the servo default.
- **default_time** (*Optional*, int): Move time used by a move without its own time, in ms.
  Defaults to `0`. **Set this to make both axes finish together**: with a shared time the servos
  interpolate over the same duration whatever the distance.

Angles are measured from each servo's own `zero_position` and `range`, so `0°` on both axes is
whatever the servos call zero. Nothing moves at boot; add an `on_boot` action if the head should
go home by itself.

## Kinematics

The head frame is +X right, +Y forward and +Z up. Positive yaw turns right, positive pitch looks
up:

```
x = cos(pitch) * sin(yaw)
y = cos(pitch) * cos(yaw)
z = sin(pitch)
```

`stackchan_motion.look_direction` inverts it, so aiming at a point only needs the direction, not
the angles. The conversion is also available as `StackchanMotion::to_direction()` and
`StackchanMotion::to_angles()` in C++.

## Sensors

```yaml
sensor:
  - platform: stackchan_motion
    stackchan_motion_id: head
    yaw:
      name: "Head yaw"
    pitch:
      name: "Head pitch"
    direction_x:
      name: "Head direction X"
    direction_y:
      name: "Head direction Y"
    direction_z:
      name: "Head direction Z"
```

- **yaw** / **pitch**: The measured angle in degrees, from the servo feedback.
- **direction_x** / **direction_y** / **direction_z**: The unit vector of the measured look
  direction. A component is `NaN` until both servos have reported.

Every field is optional and publishes from the servo feedback, so all fields share the servo read.

## Numbers

```yaml
number:
  - platform: stackchan_motion
    stackchan_motion_id: head
    yaw:
      name: "Target yaw"
    pitch:
      name: "Target pitch"
```

- **yaw** / **pitch**: The target angle in degrees. Changing one moves that axis and keeps the
  other target.
- **min_value** / **max_value** / **step** work as in any number. The component limits are applied
  on top, so a wider entity range does not let the head go past `min_yaw`/`max_yaw`.

## Buttons

```yaml
button:
  - platform: stackchan_motion
    stackchan_motion_id: head
    home:
      name: "Head home"
    left:
      name: "Head left"
    right:
      name: "Head right"
    up:
      name: "Head up"
    down:
      name: "Head down"
```

- **home**: Move to `home_yaw` / `home_pitch`.
- **left** / **right**: Move yaw by `-nudge_step` / `+nudge_step`.
- **up** / **down**: Move pitch by `+nudge_step` / `-nudge_step`.

Nudging uses the current target, so several presses in a row add up.

## Automations

```yaml
esphome:
  on_boot:
    - stackchan_motion.home: head

    - stackchan_motion.look_at:
        id: head
        yaw: 30
        pitch: -10
        time: 800

    - wait_until:
        condition:
          not:
            stackchan_motion.is_moving: head

button:
  - platform: template
    name: "Look at the door"
    on_press:
      - stackchan_motion.look_direction:
          id: head
          x: 0.7
          y: 0.7
          z: 0
```

- **stackchan_motion.look_at**: `id`, `yaw`, `pitch` (degrees), optional `speed` and `time`.
- **stackchan_motion.look_direction**: `id`, `x`, `y`, `z`, optional `speed` and `time`. The vector
  is normalized; the yaw and pitch results are then clamped to the limits.
- **stackchan_motion.nudge**: `id`, `yaw`, `pitch` deltas in degrees.
- **stackchan_motion.home**: `id`.
- **stackchan_motion.stop**: `id`. Holds the measured position on both axes.
- **stackchan_motion.is_moving**: true while either axis is moving.

All actions are non-blocking. Use `stackchan_motion.is_moving` or the servos' `on_move_done` when a
sequence has to wait for the head.
