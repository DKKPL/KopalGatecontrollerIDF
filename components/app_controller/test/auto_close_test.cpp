#include <cassert>
#include <initializer_list>

#include "app_controller/auto_close_timer.h"
#include "app_controller/gate_commands.h"
#include "hal_io/hal_io.h"

static bool inputs[static_cast<int>(Input::Reset) + 1];

bool Io::isHigh(Input input) const
{
    return inputs[static_cast<int>(input)];
}

int main()
{
    AutoCloseTimer timer;
    timer.update(false, false, 0);
    timer.update(true, false, 60000);
    assert(!timer.elapsed(80000, 20));
    assert(timer.elapsed(80001, 20));

    // Holding Open or Pedestrian, or a closing safety, restarts the delay on release.
    for (int64_t start : {100000LL, 200000LL, 300000LL}) {
        timer.update(true, true, start);
        timer.update(true, true, start + 30000);
        timer.update(true, false, start + 30010);
        assert(!timer.elapsed(start + 50010, 20));
        assert(timer.elapsed(start + 50011, 20));
    }

    timer.update(false, false, 400000);
    timer.update(true, false, 450000);
    assert(!timer.elapsed(450001, 20));
    timer.reset(460000);
    assert(!timer.elapsed(480000, 20));
    assert(timer.elapsed(480001, 20));
    timer.update(true, true, 490000);
    timer.update(true, false, 500000);
    assert(!timer.elapsed(500000, 0));
    assert(timer.elapsed(500001, 0));

    Io io;
    GateCommands commands(io);
    inputs[static_cast<int>(Input::Pedestrian)] = true;
    commands.pollPedestrian();
    inputs[static_cast<int>(Input::Pedestrian)] = false;
    commands.pollPedestrian();
    assert(commands.pedestrianHold);
    commands.openPressed = true;
    inputs[static_cast<int>(Input::Pedestrian)] = true;
    commands.pollPedestrian();
    assert(!commands.pedestrianHold);
    inputs[static_cast<int>(Input::Pedestrian)] = false;
    commands.pedestrianPressed = false;
    commands.pollPedestrian();
    assert(commands.pedestrianHold);
    assert(!commands.pedestrianPressed);
}
