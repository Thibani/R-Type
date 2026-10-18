# Loop

| File           | Content                                                  |
|----------------|----------------------------------------------------------|
| `GameLoop.hpp` | `GameLoop`, the main loop of the server and of the client |

---

## One frame

```
            ┌──────────────────────── one frame ────────────────────────┐
real time → │ frameStart() systems │ 0..N fixed steps │ frameEnd() systems │ (sleep)
            │ input, net receive   │ world.systems()  │ render, net send   │ server only
            └───────────────────────────────────────────────────────────┘
```

1. **`frameStart()`**: once per frame. Read what came from outside: keyboard,
   window events, received packets.
2. **Simulation**: the World's own systems, run once per fixed step (60 Hz by
   default, see [`Time/`](../Time/README.md)). 0 steps on a fast frame,
   several on a slow one.
3. **`frameEnd()`**: once per frame. Show or send the result: rendering,
   packets to send.

The three groups never call each other: they talk through components, like
every system. This is what keeps simulation, networking and presentation
decoupled, and lets the server simply have no rendering systems.

---

## Server and client

```cpp
// Server: sleeps until the next step instead of spinning at 100% CPU
ecs::GameLoop loop(world, {.sleepUntilNextStep = true});
loop.frameStart().add<NetReceiveSystem>(socket);
world.systems().add<MovementSystem>();
loop.frameEnd().add<NetSendSystem>(socket);
loop.run();

// Client: renders every frame; the display's vsync limits the frame rate
ecs::GameLoop loop(world);
loop.frameStart().add<InputSystem>(window);
world.systems().add<MovementSystem>();
loop.frameEnd().add<RenderSystem>(window, loop); // reads loop.alpha()
loop.run();
```

| Setting              | Default  | Meaning                                         |
|----------------------|----------|-------------------------------------------------|
| `stepSeconds`        | 1/60     | Duration of one simulation step                 |
| `maxStepsPerFrame`   | 5        | Catch-up limit after a freeze                   |
| `sleepUntilNextStep` | false    | Sleep between frames (server) or not (client)   |

Measured over one second: with sleeping, 61 frames, 60 steps, about 0% CPU.
Without sleeping, millions of frames, still 60 steps, one core at 100%.

---

## API

| Method                  | Meaning                                                    |
|-------------------------|------------------------------------------------------------|
| `GameLoop(world, settings)` | Throws `std::invalid_argument` on invalid settings     |
| `frameStart()` / `frameEnd()` | The `SystemManager` of each per-frame group          |
| `run()`                 | Runs frames until `stop()`. Exceptions from systems propagate |
| `runFrame(seconds)`     | One frame of a given duration (used by `run()` and tests)  |
| `stop()`                | Ends `run()` after the current frame. Safe from a system, another thread or a signal handler |
| `simulationTime()`      | `Time` of the last step                                    |
| `alpha()`               | 0..1 progress into the next step, for rendering            |

Simulation systems get a `Time` whose delta is always the fixed step.
Frame systems get one whose delta is the real frame duration.

Elapsed times are summed in `double` and only converted to `float` for
`Time`: a million float additions per second would drift by several percent.

Tests: [`tests/ecs/Loop/`](../../../tests/ecs/Loop).
