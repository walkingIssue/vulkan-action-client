---
name: visual-qa
description: Verify rendered output after UI, editor, viewer, Vulkan rendering, ImGui, map preview, visual lab, model loading, debug draw, screenshot, or layout changes. Use when Codex must judge whether an application visibly renders the intended state, is nonblank, shows expected assets/overlays, has readable UI, or when headless tests alone cannot prove the visual result.
---

# Visual QA

Use this skill to verify what the user would actually see. Treat screenshot inspection as required evidence for UI and rendering changes unless the environment cannot capture images.

## Workflow

1. Identify the visual surface changed: editor UI, Vulkan scene viewer, visual lab overlay, map preview, loaded model, debug lines, or layout.
2. Build the relevant target before launching it.
3. Run the app in a visible or capturable mode. Do not rely on `--headless` or `--hidden-window` for final visual judgment.
4. Capture at least one screenshot of the relevant state. For interactive work, capture before and after the action when useful.
5. Inspect the screenshot directly with the available image/browser tool before claiming success.
6. Report the screenshot path or capture method, the visible result, and any visual defects or unverified areas.
7. Stop any app process you started. Do not leave viewer/editor processes running.

## What To Check

- The frame is not blank, all-black, or stuck on a startup/default state.
- The intended panel, viewport, scene, model, primitive, or overlay is visible.
- Buttons or controls that trigger the visual state produce visible feedback.
- Text is readable and does not overlap or clip inside controls or panels.
- Debug overlays such as bounds, hitboxes, hurtboxes, paths, sockets, and grid lines are visible when expected.
- Loaded models are present, correctly framed, and not replaced by only bounds or placeholder geometry.
- Window layout does not hide the changed view behind another panel on first launch.
- Colors, scale, camera framing, and object positions are plausible for the fixture being tested.

## Repository Notes

- Hidden smoke tests and CTest process tests prove startup/data paths, not visual correctness.
- For `vulkan_scene_viewer`, prefer a visible short-frame run or a capture helper if one exists.
- For `game_editor`, verify the relevant ImGui window is visible and the action changes the displayed state.
- Save temporary screenshots under `build/<preset>/visual-qa/` or another build artifact directory.
- Do not commit generated files such as `imgui.ini`, screenshot artifacts, or test-artifact folders unless the user explicitly asks.

## If Capture Is Unavailable

If the current environment cannot capture the app window, say that visual QA is blocked, keep any nonvisual build/test verification separate, and do not present the result as visually verified.
