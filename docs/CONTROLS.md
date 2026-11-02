# Controller and touch

The mapping below is the poll in `src/platform/vita.c`. It is the build 244 client, not the control list from any other Vita port.

Sampling mode is analog. Short presses are taken from `sceCtrlPeekBufferPositive` so a tap that falls between frames is still seen. Square and the D-pad left/right edges are timestamped so a held button is not repeated by overlapping samples.

## Sticks

The left stick moves the pointer. Speed follows elapsed time and is capped so a stall cannot throw the cursor across the screen. A later touch continues from the touch point.

The right stick, past a deadzone of 60 units from center, holds the camera keys: left, right, up, and down.

While a measured remote session is armed, both sticks are forced to center and button filtering from that session applies.

## Buttons

| Input | While playing | While a menu or dialogue is open |
| --- | --- | --- |
| Cross | Primary click at the pointer | Confirm the highlighted choice |
| R | Same primary click | Confirm, and that press is not also a click |
| L | Secondary click at the pointer | Cancel the menu |
| Triangle | Run modifier, held | Run modifier, held |
| D-pad up / down | Passed to dialogue when dialogue is active | Move the highlight |
| D-pad left / right | Previous or next side-panel tab when no menu, dialogue overlay, drag, or text prompt is active | Not a tab change while those screens are up |
| Square | Toggle the performance readout | Same toggle |
| Select | Toggle the performance readout | Same toggle |
| Circle | Unused | Unused |
| Start | Unused | Unused |

Cross and R share one confirm path. The poll drops a second confirm in the same frame so dialogue does not also receive a click.

L opens the option menu by issuing the secondary button. When the menu is already visible, L cancels it instead of clicking.

D-pad left/right call the side-panel cycle (`r89_next_stone`). The comment in the poll states that this selection does not itself change the run varp.

## Touch

The front panel is the pointer. Coordinates are scaled from the touch digitizer into the framebuffer. A touch that starts in a side bar does not click a game component. The first press, a primary button, or L can open the on-screen keyboard. Submitting or dismissing that keyboard clears the pointer buttons so the same touch does not immediately click the game.

## Related files

- `src/platform/vita.c` — poll, touch, sticks, buttons
- `src/vita_button_edges.h` — edge detection for Square
- `src/r60_latest_analog.h` — newest analog sample
- `src/r63_keyboard_layout.h` — on-screen keyboard layout
- `src/r89_stone_cycle.h` — side-panel tab order
