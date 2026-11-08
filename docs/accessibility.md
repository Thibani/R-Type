# Accessibility

What the game and its documentation offer to players and developers with
disabilities, by category, and what is still missing. The subject asks for an
answer for physical and motor, audio and visual, and mental and cognitive
disabilities.

## Controls

| Action            | Key                         | Gamepad                      |
|-------------------|-----------------------------|------------------------------|
| Move              | Arrow keys by default (diagonals allowed) | Left stick or D-pad |
| Fire              | Space by default, **held** (auto-fire) | A (button 0) by default, held |
| Menu              | Esc (fixed)                  | Start (fixed)                |
| In menus          | Arrows, Tab, Enter, Esc      | Stick or D-pad, A to choose, B to go back |
| Connect           | Enter, on the connection form | A, once the name and password are typed |

Moving and firing can be bound to any key and any gamepad button in the
settings (connection screen → Settings, or Esc → Settings in game). Menus
always use the arrows, Tab, Enter and Esc, or the stick, A, B and Start, so a
player can never lock themselves out. Typing a name or password still needs the
keyboard.

## Physical and motor

| What we do | Why it helps |
|------------|--------------|
| **Hold to fire.** Holding Space fires one missile every 0.25 s; the server sets the rate, so tapping fast gives no advantage | No rapid repeated presses, which are tiring or impossible for many players |
| **Customizable controls.** Select an action in the settings and press the new key; a key already used is swapped, and "Reset controls" restores the defaults. Saved for the next start | Players choose the keys they can reach: one hand, left-handed layouts, adapted keyboards |
| **Keys are states, not events.** The client sends which keys are held 60 times per second, and every packet repeats the last 8 ticks | A key held a bit too long or too short does not matter, and a lost packet never drops an action |
| **Few keys, all on the keyboard.** Arrows, Space, Esc, Enter; no mouse needed in game | Playable with one hand, or with keyboard-emulating devices |
| **Gamepad support.** Move with the stick or D-pad, fire with a button, open the menu with Start, and use every menu with the pad. Buttons are remappable like keys, keyboard and pad work at the same time, and plugging or unplugging a pad never stops the game | Players who cannot use a keyboard comfortably, or who use an adapted controller |
| **Generous rules.** 3 hit points, and a destroyed ship comes back after 3 s, again and again | A mistake does not end the game |
| **The menu does not pause others.** Esc opens a menu while the game keeps running for the other players | A player can take a break without stopping their team |

## Audio and visual

| What we do | Why it helps |
|------------|--------------|
| **Volume control.** Sound effects from 0 (muted) to 100 %, in the settings, saved for the next start | Comfort for sound-sensitive players; muting never hides information (see below) |
| **No information is only audible.** Every sound (shot, explosion, death) comes from a server event that is also shown on screen (a missile appears, the Bydo or ship disappears, "P1 is down") | Deaf and hard-of-hearing players miss nothing |
| **Colour-blind modes.** Off, protanopia, deuteranopia or tritanopia, in the settings, saved for the next start. Each mode repaints the ships, swatches, Bydos and missiles with a palette that stays distinct in a simulation of that colour blindness (checked by a test). Every other enemy and the boss take the Bydo's colours, and the Force and its capsule the player missile's, so a friendly gadget never looks like a threat | Players can tell every element of the game apart |
| **Players told apart by colour, pattern and a P1–P4 tag**, never by colour alone | Colour-blind players can tell the four ships apart, in any mode |
| **Health is labelled and counted.** "HP" next to pips that empty one by one, whatever their colour | Health never depends on telling green from red |
| **Missiles told apart by shape.** Player missiles are long comets, enemy missiles are round balls | Telling a threat from a friendly shot never depends on colour |
| **Hyperlegible numbers.** Scores, P1–P4 tags and player names use Atkinson Hyperlegible; Bebas Neue is kept for titles, because it is condensed and has few accents | Tags and scores can be read at a glance, including with low vision |
| **Interface size.** 75, 100, 125 or 150 %, in the settings, saved for the next start. Menus, the HUD and ship tags change size, separately from the window, and the arena keeps its proportions | Larger text and controls for low vision or a small or distant screen; more room for the game on a large one |
| **The window scales** to any size, with letterboxing that keeps the proportions | Large screens and zoomed displays work |
| **Unknown entities are drawn as a "?"** instead of being hidden | Nothing on screen is ever invisible |

## Mental and cognitive

| What we do | Why it helps |
|------------|--------------|
| **One goal, simple rules.** Shoot the Bydos, avoid their missiles; one shared team score, no individual ranking | Little to remember, no competitive pressure between teammates |
| **Predictable enemies.** Bydos always enter from the right and follow the same wave; their shots fly straight | Patterns can be learned |
| **No time limit, no game over yet.** The game keeps going until the players leave | No pressure to succeed on the first try |
| **Clear connection errors.** A rejected or lost connection says why (server full, wrong version, timeout) | The player knows what to do next |

## Documentation

- Plain Markdown, with real headings, so screen readers and the GitHub outline
  can jump between sections.
- Tables always have header rows; links say where they lead.
- Every diagram (ASCII art) is also explained in the text around it, so
  nothing is only visual.
- English throughout, short sentences, one idea per paragraph.

## Still missing

| Need | Plan |
|------|------|
| Adjustable game speed or difficulty | Not planned |
| Larger text | Interface scaling (#123) |
| High-contrast theme | Not scheduled |
| Visual cue for incoming fire | Not scheduled |
