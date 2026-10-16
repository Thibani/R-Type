# Time

| File                | Content                                                      |
|---------------------|--------------------------------------------------------------|
| `Time.hpp`          | `Time`, the timing information given to every system         |
| `FixedTimestep.hpp` | `FixedTimestep`, turns real frame durations into fixed steps |
| `FrameClock.hpp`    | `FrameClock`, measures the real time between two frames      |

---

## Why?

Gameplay must not depend on CPU speed. This is wrong:

```cpp
position.x += 5; // 5 units PER FRAME
```

At 30 FPS the ship moves 150 units per second; at 144 FPS it moves 720. On the
server, the speed would depend on the machine.

The right way is to express speeds **per second** and to scale them by the
duration of the frame:

```cpp
position.x += velocity.dx * time.deltaSeconds(); // velocity.dx units PER SECOND
```

| Frame rate | `deltaSeconds()` | Movement per frame (dx = 100) | Movement per second |
|------------|------------------|-------------------------------|---------------------|
| 10 FPS     | 0.1              | 10                            | 100                 |
| 60 FPS     | ≈ 0.0167         | ≈ 1.67                        | 100                 |
| 144 FPS    | ≈ 0.0069         | ≈ 0.69                        | 100                 |

---

## API

| Method                  | Meaning                                                    |
|-------------------------|------------------------------------------------------------|
| `Time()`                | First frame: delta = 0, elapsed = 0                        |
| `Time(delta, elapsed)`  | Builds a time explicitly                                   |
| `deltaSeconds()`        | Duration of the current frame, in seconds                  |
| `elapsedSeconds()`      | Total time since the start of the simulation, in seconds   |
| `advanced(delta)`       | Returns the time of the next frame: `Time{delta, elapsed + delta}` |

Everything is `constexpr` and `noexcept`.

---

## Design: a value, not a clock

`Time` **does not read any clock**. It is a plain value passed to systems:

```cpp
ecs::Time time;
while (running) {
    const float delta = measureFrameDuration(); // done by the main loop
    time = time.advanced(delta);
    world.update(time);
}
```

Benefits:

- **Deterministic**: the same sequence of deltas always produces the same
  result.
- **Testable**: a test chooses its deltas, no need to wait for real time.
- **Flexible**: the server can use a fixed tick (e.g. `advanced(1.0F / 60.0F)`
  every tick) while the client uses the real frame duration.

Measuring real time belongs to the main loop of the client and of the server,
not to the systems. The two helpers below are for that main loop.

---

## Fixed timestep: same simulation on every machine

Even with `deltaSeconds()`, a variable delta changes the result: a collision
checked every 7 ms and one checked every 33 ms do not see the same overlaps.
The server, and every client predicting it, must therefore update with the
**same** delta every time, 60 times per second.

`FixedTimestep` turns irregular frames into a whole number of fixed steps.
Time that does not fill a step is kept for the next frame:

```cpp
ecs::FixedTimestep timestep(1.0F / 60.0F); // 60 updates per second
ecs::FrameClock clock;
ecs::Time time;

while (running) {
    for (int i = timestep.advance(clock.tick()); i > 0; --i) {
        time = time.advanced(timestep.stepSeconds());
        world.update(time);
    }
    render(timestep.alpha()); // client only: 0..1 between the last two steps
}
```

| Real frame (60 Hz step = 16.7 ms) | `advance()` returns | Kept for next frame |
|-----------------------------------|---------------------|---------------------|
| 7 ms (144 FPS)                    | 0 most frames, then 1 | the remainder     |
| 16.7 ms (60 FPS)                  | 1                   | ≈ 0                 |
| 40 ms (25 FPS)                    | 2                   | ≈ 6.7 ms            |
| 3 s (debugger pause)              | 5 (`maxStepsPerFrame`) | 0: backlog dropped |

The cap avoids the *spiral of death*: a slow machine that tries to catch up
runs even more updates per frame, gets even slower, and never recovers.
Above the cap the game slows down instead.

| Method                               | Meaning                                                |
|--------------------------------------|--------------------------------------------------------|
| `FixedTimestep(step, maxSteps = 5)`  | Throws `std::invalid_argument` if either is not > 0    |
| `advance(frameSeconds)`              | Adds a frame, returns the steps to run now (0..maxSteps). Negative or NaN counts as 0 |
| `stepSeconds()`                      | The fixed delta of every update                        |
| `alpha()`                            | Progress into the next step, 0..1, for render interpolation |

`FixedTimestep` reads no clock either, so it is `constexpr` and tested with
exact durations. Its accumulator is a `double`: with float, the tiny frames of
an uncapped client were rounded off and whole steps were lost.

The server and the client do not write this loop themselves: they use
[`ecs::GameLoop`](../Loop/README.md), which runs it with the frame systems.

## FrameClock

`FrameClock::tick()` returns the seconds since the previous `tick()`, using
`std::chrono::steady_clock` (it never jumps when the system time is changed).
`BasicFrameClock<Clock>` accepts any clock with a `now()`, so tests use a fake
one instead of sleeping.

Tests: [`tests/ecs/Time/`](../../../tests/ecs/Time).
