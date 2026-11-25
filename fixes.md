# Aanpassingen in de poortlogica

Onderstaande dngen werden gevonden in de code van de gate

## 1. Coil 9 (autoclose) doet niets

`pollAutoClose()` zet elke while loop de state van de autoclose-schakelaar in `autoClose`.
Een waaarde die voor autoclose gezet wordt via modbus wordt daardoor direct overschreden

Oplossing: de Modbus-stand apart bijhouden. Autoclose staat aan als de schakelaar
of Modbus hem aanzet. Door dat `autoclose schakelaar OR modbus value`te doen kunnen we dat probleem oplossen

`include/app_controller/gate_commands.h`, onder `autoClose`:
```cpp
bool remoteAutoClose = false;    ///< autoclose aangezet via Modbus (coil 9)
```

`src/gate_commands.cpp`, `pollAutoClose()` (regel 56):
```cpp
return autoClose = !io.isHigh(Input::AutoClose) || remoteAutoClose;
```

`src/gate_commands.cpp`, `applyCommand()` (regel 74):
```cpp
case GateCommand::ToggleAutoClose:
    remoteAutoClose = !remoteAutoClose;
    autoClose = !io.isHigh(Input::AutoClose) || remoteAutoClose;
    break;
```

## 2. Modbus Open tijdens sluiten wordt genegeerd

`stateClosing()` kijkt alleen naar de echte knop. De poort sluit volledig en gaat
daarna vanzelf weer open als we dit willen aansturen met modbus. Dit is doordat de waarde latchedOpen niet gecheckt wordt by stateClosing.
Deze moet dus toegeovegd worden om dat op te lossen

`src/gate_controller.cpp`, regel 271:
```cpp
if (commands.latchedOsc || commands.openPressed || commands.latchedOpen || lscActivated) {
    ESP_LOGI(TAG, "Stop/limit triggered");
    commands.openPressed = false;
    commands.latchedOpen = false;
    commands.latchedOsc = false;
```

## 3. Modbus Close tijdens openen wordt genegeerd

Zelfde probleem in `stateOpening()`: de poort opent volledig en sluit daarna vanzelf. Dezelfde aanpassing is dus hier ook nodig.

`src/gate_controller.cpp`, regel 490:
```cpp
if (commands.latchedOsc || commands.closePressed || commands.latchedClose || lsoActivated) {
    ESP_LOGI(TAG, "Stop/limit triggered");
    commands.closePressed = false;
    commands.latchedClose = false;
    commands.latchedOsc = false;
```

## 4. Modbus Pedestrian tijdens bewegen wordt genegeerd

Zelfde probleem: de poort rijdt tot het einde en gaat daarna vanzelf naar de
Pedestrian waarde. 

`src/gate_controller.cpp`, regel 308 in `stateClosing()`:
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
    case -1:   // open gate to pedestrian: beide vlaggen blijven staan, stateClosingNormalStop() gebruikt ze
        pedestrianMode = true;
        setMainLoopStep(MainLoopStep::ClosingNormalStop, "Pedestrian mode opening");
        break;
    }
}
```

`src/gate_controller.cpp`, regel 523 in `stateOpening()`:
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

## 5. Richting van pin 37 omgekeerd

De pulsteller en de backtrack lezen pin 37 hoog = openen. De vlag `actualDirectionOpening` leest laag = openen. Daardoor tonen Modbus
register 21 en de CLI `dir` de verkeerde richting. Dit is enkel een vermoeden. Kan dit nog eens bekeken worden? 

`src/gate_controller.cpp`, regel 161:
```cpp
actualDirectionOpening = directionHigh;   // was !directionHigh
```

`src/gate_controller.cpp`, regel 1013, zodat de teller hetzelfde blijft tellen:
```cpp
if (effectiveDirection) {                 // was !effectiveDirection
    position += actualCount;
```
