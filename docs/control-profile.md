# Control Profile

The scene viewer loads `config/controls/player_control_profile.json` at startup and watches the file timestamp while running. When the file changes, it parses a new profile, resolves key names, and swaps the profile only if the whole reload succeeds. A bad edit keeps the last good profile active.

## Runtime Shape

```text
JSON control profile
  -> ControlProfile
  -> combat::MovementTuning
  -> fixed-tick movement functions

JSON binding names
  -> resolved GLFW key codes in the viewer
  -> normalized LocalMoveIntent / action booleans
  -> fixed-tick movement functions
```

The combat layer receives typed tuning and normalized intent. It does not read files, parse JSON, or know about GLFW key codes.

## Movement

The current movement values are:

```json
{
  "player_run_speed_world_units_per_second": 37.7,
  "player_sprint_speed_scale": 2.3,
  "backpedal_speed_scale": 0.33333334
}
```

Sprint scales run speed unless the player is backpedaling. Backpedal uses normal run speed times `backpedal_speed_scale`.

## Bindings

Bindings are arrays of key names. Examples:

```json
{
  "camera_mode_toggle": ["TAB", "CAPS_LOCK"],
  "player_sprint": ["LEFT_SHIFT", "RIGHT_SHIFT"]
}
```

Supported key names currently cover letters, digits, arrows, Escape, Tab, Caps Lock, Shift, Control, Alt, Space, and Enter. Add more names in the viewer resolver when a new action needs them.
