# Input Pipeline Notes

Input should enter the simulation as timestamped player intent, not as ad hoc render-frame state.

The current viewer uses GLFW because it is a small cross-platform way to create a window and Vulkan surface. That is fine for the bootstrap renderer, but combat input should get its own layer before it touches gameplay state.

## Windows Reality

Most app-level input is delivered through an operating-system queue. On Win32, the usual path is a message pump. GLFW wraps that pump for us. If we only ask GLFW for key state once per render frame, then input sampling is effectively tied to the render loop.

That does not mean we need to read USB ports directly. For normal keyboards, mice, and gamepads, the better Windows APIs are:

- Raw Input for keyboard and mouse scancodes, mouse deltas, and some HID devices.
- XInput for Xbox-compatible controllers.
- GameInput for modern Microsoft game input when available.
- HID/SetupAPI/WinUSB only for special devices where we own the protocol.

DirectInput exists and is COM-based, but it is legacy for most modern game use. COM is still alive on Windows, but "COM object" is not automatically lower latency or more correct than Raw Input/GameInput.

For the first controller path, use XInput. It is narrow, stable, and enough for Xbox-compatible pads while we are still building the combat input model.

The current scene viewer has the first thin version of this: XInput left stick and `WASD` drive camera-relative player strafing, mouse controls the player-follow camera, and right mouse button locks player facing to the camera. This is prototype plumbing, not the final combat input API.

## Desired Engine Shape

The platform layer should collect input events as soon as the OS delivers them:

```text
windows message pump / raw input / gamepad polling
  -> timestamped InputEvent ring buffer
  -> fixed-tick InputSnapshot builder
  -> combat state machine commands
```

The combat state machine should consume fixed-tick snapshots or commands:

```cpp
struct InputEvent {
    PlayerId player;
    InputControl control;
    InputAction action;
    double timestampSeconds;
};

struct CombatInput {
    bool lightAttack;
    bool heavyAttack;
    bool dodge;
    glm::vec2 move;
    glm::vec2 look;
};
```

That gives us clean edge detection, buffering windows, cancel checks, replay, rollback, and debugging. Rendering can run faster or slower without changing whether a dodge or attack happened.

Movement axes should start local, not global. `WASD` and stick input describe side and forward intent. The control scheme chooses a frame, such as camera yaw for strafing, character facing for committed action movement, or a lock-on target frame for duels. Prototype backpedal is a combat rule: negative forward intent snaps facing to the camera frame and moves at one-third forward speed.

## Practical First Step

For the prototype, keep GLFW for the window. Add a platform input module that records GLFW callbacks into an input queue, then later replace or supplement that module with Win32 Raw Input without changing the combat state machine API.

When we outgrow GLFW callbacks, the Windows-specific path should use Raw Input with `RegisterRawInputDevices` and `WM_INPUT`, plus XInput for controllers. GameInput can come later if we need broader device support. Direct USB/HID access should be reserved for custom hardware.
