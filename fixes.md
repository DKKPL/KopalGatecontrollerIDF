# Gate logic fixes C1–C4 (Modbus commands)

**Change request for the gate logic owner.** These changes are in the gate logic, not in the
Modbus code, so the Modbus team does not make them.

| | Problem | File |
|---|---|---|
| C1 | Coil 9 (auto-close) has no effect | `gate_commands.cpp`, `gate_commands.h` |
| C2 | Modbus Open while closing: ignored, the gate opens by itself later | `gate_controller.cpp` |
| C3 | Modbus Close while opening: ignored, the gate closes by itself later | `gate_controller.cpp` |
| C4 | Modbus Pedestrian while moving: ignored, the gate moves by itself later | `gate_controller.cpp` |

All files are in `components/app_controller/`.

---

## C1: Coil 9 (auto-close) has no effect

### The problem

1. `commands.autoClose` decides whether the gate closes by itself.
2. Every loop (about every 10 ms) `pollAutoClose()` copies the auto-close **switch** into it
   (`gate_commands.cpp:56`, called from `gate_controller.cpp:190`).
3. Modbus coil 9 sends `GateCommand::ToggleAutoClose`, which flips `autoClose`
   (`gate_commands.cpp:75`).
4. In the next loop, step 2 copies the switch back over it. Coil 9 has no effect.

### The fix

Keep the Modbus setting in its own flag. Auto-close is then on when **the switch or Modbus**
turns it on. The Modbus side does not change: coil 9 still sends `ToggleAutoClose`.

`include/app_controller/gate_commands.h`, next to `autoClose` (line 54):
```cpp
bool autoClose = false;          ///< auto-close on: switch or Modbus (bAutoClose)
bool remoteAutoClose = false;    ///< auto-close turned on over Modbus (coil 9)
```

`src/gate_commands.cpp`, `pollAutoClose()` (line 56).

Before:
```cpp
// Active low.
bool GateCommands::pollAutoClose()
{
    return autoClose = !io.isHigh(Input::AutoClose);
}
```

After:
```cpp
// Active low. Modbus can also turn auto-close on (coil 9).
bool GateCommands::pollAutoClose()
{
    return autoClose = !io.isHigh(Input::AutoClose) || remoteAutoClose;
}
```

`src/gate_commands.cpp`, `applyCommand()` (line 74).

Before:
```cpp
case GateCommand::ToggleAutoClose:
    autoClose = !autoClose;
    break;
```

After:
```cpp
case GateCommand::ToggleAutoClose:
    remoteAutoClose = !remoteAutoClose;
    autoClose = !io.isHigh(Input::AutoClose) || remoteAutoClose;
    break;
```

| Switch | Coil 9 (Modbus) | Auto-close |
|---|---|---|
| off | off | off |
| off | on | **on** |
| on | off | on |
| on | on | on |

Notes:
- With the switch on, Modbus cannot turn auto-close off. If that is needed, the owner has to
  pick another rule, for example "the last change wins".
- `remoteAutoClose` is RAM only, so it is off again after a reboot. Say so if it has to be
  saved.

---

# C2–C4: Modbus Open, Close and Pedestrian while the gate moves

File: `components/app_controller/src/gate_controller.cpp`

## The problem

A command reaches the gate in one of two flags:

| Source | Open | Close | Pedestrian |
|---|---|---|---|
| Real button | `commands.openPressed` | `commands.closePressed` | `commands.pedestrianPressed` |
| Modbus coil | `commands.latchedOpen` | `commands.latchedClose` | `commands.latchedPedestrian` |

- The **stopped** states check both flags.
- `stateClosing()` and `stateOpening()` only check the real button flag.

So a Modbus command sent while the gate moves:

1. is ignored: the gate keeps moving to the end;
2. stays latched;
3. is carried out by the stopped state once the gate has stopped, so the gate moves again by itself.

| | Command while moving | Real button | Modbus coil (now) |
|---|---|---|---|
| C2 | Open while closing | stops at once | closes fully, then **opens by itself** |
| C3 | Close while opening | stops at once | opens fully, then **closes by itself** |
| C4 | Pedestrian while closing or opening | goes to the pedestrian position | moves to the end, then **goes to the pedestrian position by itself** |

## The fix

The fix is the same in all three places:

1. Add the latched flag to the `if`.
2. Clear it wherever the button flag is cleared. If it is not cleared, it stays set and the
   gate still moves by itself after it stops.

---

### C2: Open while closing

`stateClosing()`, line 271.

Before:
```cpp
// 2. Stop/limit transition: exit immediately if triggered
if (commands.latchedOsc || commands.openPressed || lscActivated) {
    ESP_LOGI(TAG, "Stop/limit triggered");
    commands.openPressed = false;
    commands.latchedOsc = false;
```

After:
```cpp
// 2. Stop/limit transition: exit immediately if triggered
if (commands.latchedOsc || commands.openPressed || commands.latchedOpen || lscActivated) {
    ESP_LOGI(TAG, "Stop/limit triggered");
    commands.openPressed = false;
    commands.latchedOpen = false;
    commands.latchedOsc = false;
```

The rest of the block stays the same.

---

### C3: Close while opening

`stateOpening()`, line 490.

Before:
```cpp
// 2. Stop/limit transition: exit immediately if triggered
if (commands.latchedOsc || commands.closePressed || lsoActivated) {
    ESP_LOGI(TAG, "Stop/limit triggered");
    commands.closePressed = false;
    commands.latchedOsc = false;
```

After:
```cpp
// 2. Stop/limit transition: exit immediately if triggered
if (commands.latchedOsc || commands.closePressed || commands.latchedClose || lsoActivated) {
    ESP_LOGI(TAG, "Stop/limit triggered");
    commands.closePressed = false;
    commands.latchedClose = false;
    commands.latchedOsc = false;
```

---

### C4: Pedestrian while closing or opening

There are two places: one in `stateClosing()` and one in `stateOpening()`.

#### Closing: `stateClosing()`, line 308

Before:
```cpp
// 5. Pedestrian mode handling
if (commands.pedestrianPressed) {
    switch (checkPedestrianDirection()) {
    case 1:    // close gate to pedestrian
        pedestrianMode = true;
        commands.pedestrianPressed = false;
        setMainLoopStep(MainLoopStep::Closing, "Pedestrian mode closing");
        break;
    case 0:    // close gate completely
        commands.pedestrianPressed = false;
        setMainLoopStep(MainLoopStep::ClosingNormalStop, "Pedestrian mode closing complete");
        break;
    case -1:   // open gate to pedestrian (pedestrianPressed stays set, as in C#)
        pedestrianMode = true;
        setMainLoopStep(MainLoopStep::ClosingNormalStop, "Pedestrian mode opening");
        break;
    }
}
```

After:
```cpp
// 5. Pedestrian mode handling
if (commands.pedestrianPressed || commands.latchedPedestrian) {
    switch (checkPedestrianDirection()) {
    case 1:    // close gate to pedestrian
        pedestrianMode = true;
        commands.pedestrianPressed = false;
        commands.latchedPedestrian = false;
        setMainLoopStep(MainLoopStep::Closing, "Pedestrian mode closing");
        break;
    case 0:    // close gate completely
        commands.pedestrianPressed = false;
        commands.latchedPedestrian = false;
        setMainLoopStep(MainLoopStep::ClosingNormalStop, "Pedestrian mode closing complete");
        break;
    case -1:   // open gate to pedestrian (both flags stay set for the stop state)
        pedestrianMode = true;
        setMainLoopStep(MainLoopStep::ClosingNormalStop, "Pedestrian mode opening");
        break;
    }
}
```

`case -1` does not clear the flags. That is on purpose and the same as today for the button:
`stateClosingNormalStop()` (line 460) checks both flags, clears both, and opens to the
pedestrian position.

#### Opening: `stateOpening()`, line 523

Before:
```cpp
// 5. Pedestrian mode handling
if (commands.pedestrianPressed) {
    switch (checkPedestrianDirection()) {
    case 1:    // close gate to pedestrian
        pedestrianMode = true;
        commands.pedestrianPressed = false;
        setMainLoopStep(MainLoopStep::OpeningNormalStop, "Pedestrian mode closing");
        break;
    case 0:    // close gate completely
        commands.pedestrianPressed = false;
        setMainLoopStep(MainLoopStep::OpeningNormalStop, "Pedestrian mode closing complete");
        break;
    case -1:   // open gate to pedestrian
        pedestrianMode = true;
        commands.pedestrianPressed = false;
        setMainLoopStep(MainLoopStep::Opening, "Pedestrian mode opening");
        break;
    }
}
```

After:
```cpp
// 5. Pedestrian mode handling
if (commands.pedestrianPressed || commands.latchedPedestrian) {
    switch (checkPedestrianDirection()) {
    case 1:    // close gate to pedestrian
        pedestrianMode = true;
        commands.pedestrianPressed = false;
        commands.latchedPedestrian = false;
        setMainLoopStep(MainLoopStep::OpeningNormalStop, "Pedestrian mode closing");
        break;
    case 0:    // close gate completely
        commands.pedestrianPressed = false;
        commands.latchedPedestrian = false;
        setMainLoopStep(MainLoopStep::OpeningNormalStop, "Pedestrian mode closing complete");
        break;
    case -1:   // open gate to pedestrian
        pedestrianMode = true;
        commands.pedestrianPressed = false;
        commands.latchedPedestrian = false;
        setMainLoopStep(MainLoopStep::Opening, "Pedestrian mode opening");
        break;
    }
}
```

---

## How to test

Send each command with Modbus FC05 (`0xFF00`). For coils 1, 2 and 4, send it **while the gate
moves**, then compare with the real button.

| Test | Expected after the fix |
|---|---|
| Coil 9 with the auto-close switch off | Auto-close turns on: the gate closes by itself after the auto-close delay. |
| Coil 9 again | Auto-close turns off again. |
| Coil 9 with the switch on | Auto-close stays on (the switch wins). |
| Coil 1 (Open) while closing | The gate stops at once, the same as the Open button. It does **not** open later by itself. |
| Coil 1 again | The gate opens. |
| Coil 2 (Close) while opening | The gate stops at once. It does **not** close later by itself. |
| Coil 4 (Pedestrian) while closing, above the pedestrian position | The gate stops at the pedestrian position. |
| Coil 4 while closing, below the pedestrian position | The gate stops, then opens to the pedestrian position. |
| Coil 4 while opening, below the pedestrian position | The gate stops at the pedestrian position. |
| Coil 4 while opening, above the pedestrian position | The same as the Pedestrian button in that position. |
| All of the above with the real buttons | No change from today. |

Test it on a gate where the limit switches really change. On the bench the open limit input
reads "open" all the time, and that hides the "moves by itself later" problem.
