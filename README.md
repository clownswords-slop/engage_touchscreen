# Fire Emblem Engage Touchscreen Support (work in progress)

Adds touchscreen / mouse support to Fire Emblem Engage (v2.0.0), built the same way as the Three Houses mod in
`../claude_touchmod`: a standalone `subsdk9` (no skyline / Cobalt needed) plus a patched `main.npdm`.

## Install (Ryujinx)
Copy `exefs/subsdk9` and `exefs/main.npdm` to
`%APPDATA%\Ryujinx\mods\contents\0100a6301214e000\EngageTouchMod\exefs\`. Turn "Direct Mouse Access" off so mouse clicks
arrive as touches.

## What works so far
- Every standard menu (title, options, dialogs, unit command / item menus, ...): tap an entry to select it, tap the selected
  entry again to confirm. Drag up / down on a menu to scroll. Tap well outside a menu to go back (B). On option rows, tap the
  arrows to change the value.
- Button prompts shown on screen (e.g. "+ menu" on the map): tap them to press that button.
- Title screen: tap to continue.
- Dialogue: tap to advance. Cutscene movies: tap to skip (+).
- Tutorial windows: tap the arrows to turn pages, anywhere else to continue.
- Battle map: tap a tile to move the cursor there, tap the cursor's tile to select / confirm (unit, destination).
  Target selection: tap an enemy to target it, tap it again to attack.

- Details mode (X): tap a stat, item or skill to see its description; tap elsewhere to close it.
- Unit status screen: tap away from the prompts to go back; after + View hid the panels, tap to bring them back.
- Battle forecast: tap the "(A) Attack" button (or the target) to attack. Engage confirmation: tap the "(A) Engage" button.
- Combat animations: tap to skip (when the game allows skipping). "STAGE CLEAR!": tap to continue.
- Page arrows beside lists (Guide, Units): tap them to turn the page.
- Units list: tap a column header to sort by it, tap it again to flip the order; tap a unit to leave sorting.
- Battle forecast: tap the "< L weapon R >" arrows to change the weapon. With two enemies in range, tap the other one to switch
  targets (tap it again to attack).

- Battle map: drag up / down to tilt the camera (drag down = lower and closer).
- Explore fields: drag to turn the camera; tap the "(A) Talk" label over someone to talk to them; tap the corner prompts.
- Settings: a new "Touchscreen" entry with Horizontal / Vertical Camera Sensitivity.
- Storehouse: tap a category icon (or the arrows) to switch category. Bond popups, "STAGE CLEAR!" and "GAME OVER": tap to continue.

## Not done yet
See `HANDOFF.md`.
