# Time

| File       | Content                                                  |
|------------|----------------------------------------------------------|
| `Time.hpp` | `Time`, the timing information given to every system     |

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
not to the ECS.

Tests: [`tests/ecs/Time/`](../../../tests/ecs/Time).
