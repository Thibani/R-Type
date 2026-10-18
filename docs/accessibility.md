# Accessibility

What the game and its documentation offer to players and developers with
disabilities, by category, and what is still missing. The subject asks for an
answer for physical and motor, audio and visual, and mental and cognitive
disabilities.

## Controls

| Action            | Key                         |
|-------------------|-----------------------------|
| Move              | Arrow keys (diagonals allowed) |
| Fire              | Space, **held** (auto-fire)  |
| Menu / back       | Esc                          |
| Connect           | Enter, on the connection form |

## Physical and motor

| What we do | Why it helps |
|------------|--------------|
| **Hold to fire.** Holding Space fires one missile every 0.25 s; the server sets the rate, so tapping fast gives no advantage | No rapid repeated presses, which are tiring or impossible for many players |
| **Keys are states, not events.** The client sends which keys are held 60 times per second, and every packet repeats the last 8 ticks | A key held a bit too long or too short does not matter, and a lost packet never drops an action |
| **Few keys, all on the keyboard.** Arrows, Space, Esc, Enter; no mouse needed in game | Playable with one hand, or with keyboard-emulating devices |
| **Generous rules.** 3 hit points, and a destroyed ship comes back after 3 s, again and again | A mistake does not end the game |
| **The menu does not pause others.** Esc opens a menu while the game keeps running for the other players | A player can take a break without stopping their team |

## Audio and visual

| What we do | Why it helps |
|------------|--------------|
| **No information is only audible.** Every sound (shot, explosion, death) comes from a server event that is also shown on screen (a missile appears, the Bydo or ship disappears, "P1 is down") | Deaf and hard-of-hearing players miss nothing |
| **Players told apart by colour, pattern and a P1–P4 tag**, never by colour alone | Colour-blind players can tell the four ships apart |
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

## Missing, planned for Part 2

| Need | Plan |
|------|------|
| Remapping keys, gamepad support | Key bindings stored in a settings file, editable in the menu |
| Volume and mute settings | Separate music / effects sliders in the menu |
| Adjustable game speed or difficulty | Server-side rules (#87, game modes) |
| Larger text, high-contrast theme | Interface scale and theme options |
| Visual cue for incoming fire | Highlight Bydos about to shoot |
