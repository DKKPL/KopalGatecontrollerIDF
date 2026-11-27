#ifndef APP_CONTROLLER_GATE_CONTROLLER_H
#define APP_CONTROLLER_GATE_CONTROLLER_H

#include <cstdint>
#include <functional>
#include <mutex>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "app_controller/auto_close_timer.h"
#include "app_controller/gate_commands.h"
#include "app_controller/gate_outputs.h"
#include "app_controller/gate_types.h"
#include "app_controller/teach.h"
#include "motor_control/move_trace.h"
#include "motor_control/ramp_generator.h"
#include "motor_control/safety.h"

class Io;
class Motor;
class Counter;
enum class Direction : uint8_t;
class PublicSettings;
class AdvancedSettings;
class TeachSettings;
class ConfigStorage;

/**
 * @brief The gate state machine: decides when the gate opens, stops and closes
 *        (was the static partial class Program). Runs in its own FreeRTOS task.
 */
class GateController {
public:
    /**
     * @brief Hardware and settings are created in main.cpp and must outlive the controller.
     */
    GateController(Io& io, Motor& motor, Counter& counter,
                   const PublicSettings& pub, AdvancedSettings& adv,
                   TeachSettings& teach, ConfigStorage& store);

    /**
     * @brief Start the gate task; from then on the gate runs by itself (was Main).
     */
    void start();
    /**
     * @brief Ask the gate to do something (open, close, ...). Safe to call from any task.
     * @return false when the command queue is full.
     */
    bool post(GateCommand cmd);
    /**
     * @brief A copy of the gate's current state. Safe to call from any task.
     */
    GateStatus getStatus() const;
    /**
     * @brief The recording of the last movement, for the /trace page.
     */
    const MoveTrace& getTrace() const;
    /**
     * @brief Function to call each time the gate is fully closed (wifi uses it to switch off).
     */
    void setCompleteCloseHandler(std::function<void()> handler);
    /**
     * @brief Function to call at every (re)start of the state machine in run() (wifi reads the Wi-Fi switch there).
     */
    void setEntryPointHandler(std::function<void()> handler);

private:
    friend class TeachSequence;

    static constexpr int kTaskStack = 8192;
    static constexpr UBaseType_t kTaskPriority = 10;
    static constexpr int kQueueLength = 16;

    /**
     * @brief FreeRTOS entry point; calls run() on the controller passed as arg.
     */
    static void taskEntry(void* arg);
    /**
     * @brief The gate task: check the reset button once, then forever teach or find the position
     *        and run normalOperation(), again after each ops-limit restart (was Main + EntryPoint).
     */
    void run();
    /**
     * @brief The 10 ms loop: read inputs, handle safeties, run the current state
     *        (was State_NormalOperation).
     */
    void normalOperation();

    /**
     * @brief Closing: watch safeties and stop/limit, keep the motor running (was State_Closing).
     */
    void stateClosing();
    /**
     * @brief Stopped after closing: finish the stop, then wait for the next command or auto-close
     *        (was State_Closing_normal_stop).
     */
    void stateClosingNormalStop();
    /**
     * @brief Opening: watch safeties and stop/limit, keep the motor running (was State_Opening).
     */
    void stateOpening();
    /**
     * @brief Stopped after opening: finish the stop, then wait for the next command or auto-close
     *        (was State_Opening_normal_stop).
     */
    void stateOpeningNormalStop();
    /**
     * @brief Photocell or torque limit tripped while closing: back off or wait, as configured
     *        (was State_Safety_Close_Aft_Closing).
     */
    void stateSafetyCloseAfterClosing();
    /**
     * @brief Torque limit tripped while opening: back off or wait, as configured
     *        (was State_Safety_Open_Aft_Opening).
     */
    void stateSafetyOpenAfterOpening();
    /**
     * @brief Safety edge pressed while closing: stop, wait until clear, then continue as configured
     *        (was State_Safety_Open_Close_Aft_Closing).
     */
    void stateSafetyOpenCloseAfterClosing();
    /**
     * @brief Safety edge pressed while opening: stop, wait until clear, then continue as configured
     *        (was State_Safety_Open_Close_Aft_Opening).
     */
    void stateSafetyOpenCloseAfterOpening();
    /**
     * @brief Go to another state; reason is only for the log (was SetMainLoopStep).
     */
    void setMainLoopStep(MainLoopStep newStep, const char* reason);
    /**
     * @brief Which movement the current safety state interrupted (for the back-off direction).
     */
    Interrupted interruptedMovement() const;

    /**
     * @brief Unlock if needed, switch the light on and start the motor in direction (was StartMotor).
     */
    void startMotor(Direction direction);
    /**
     * @brief Stop the motor; forceStop also flips the direction output (was StopMotor).
     */
    void stopMotor(bool forceStop = false);
    /**
     * @brief Pick speed and torque for the current state and position: ramp up, slow down near
     *        the end, extra torque zone near closed (was SetMotorSpeed).
     */
    void setMotorSpeed();
    /**
     * @brief Restart the state machine when a movement takes longer than the max run time (was CheckOpsLimitReached).
     */
    void checkMaxRunTimeExceeded();
    /**
     * @brief Read the encoder: update the position and/or the RPM, stop at the pedestrian position
     *        (was UpdatePositionAndRpm).
     * @return the RPM.
     */
    int updatePositionAndRpm(bool updateRpm = true, bool updatePosition = true);
    /**
     * @brief Read the open limit switch; at the limit the position is set to the taught open position.
     */
    bool isLsoActivated();
    /**
     * @brief Read the close limit switch; at the limit the position is set to 0.
     */
    bool isLscActivated();

    /**
     * @brief The pedestrian opening in encoder pulses.
     */
    int64_t pedestrianPulses() const;
    /**
     * @brief Which way to go for the pedestrian opening: 1 = close, -1 = open, 0 = already there.
     */
    int checkPedestrianDirection() const;
    /**
     * @brief True when the gate is between closed and the pedestrian opening.
     */
    bool checkPedestrianZone() const;
    /**
     * @brief True when the gate is at the pedestrian opening (within the margin).
     */
    bool isInPedestrianArea() const;
    /**
     * @brief Switch pedestrian mode on while the button is held, off at the pedestrian opening.
     */
    void updatePedestrianMode();

    /**
     * @brief Read all safety inputs and clear a torque alarm if one is on (was PollSafetyInputs).
     */
    void pollSafetyInputs();
    /**
     * @brief If a safety is tripped: stop, wait until all are clear, start again in direction
     *        (was EnsureSafetyAndResume).
     * @return false when it had to wait.
     */
    bool ensureSafetyAndResume(Direction direction);
    /**
     * @brief Wait until all safeties are clear, remembering button presses meanwhile
     *        (was WaitForAllSafetiesOkAndLatchInputs).
     */
    void waitForAllSafetiesOkAndLatchInputs();
    /**
     * @brief Read the buttons and remote commands and remember any press (was PollAndLatchInputCommands).
     */
    void pollAndLatchInputCommands();
    /**
     * @brief Handle all commands that other tasks sent with post() (was PollRemoteCommands).
     */
    void drainCommands();
    void updateAutoCloseTimer();

    /**
     * @brief Lock the gate once it has been still long enough, or right away when fully closed.
     */
    void engageLockAfterMotorStop();
    /**
     * @brief Switch the passage light off once its delay has passed, or right away when fully closed.
     */
    void passageLightOffAfterMotorStop();

    /**
     * @brief Copy the current state into the snapshot that getStatus() returns.
     */
    void publishStatus();

    // Hardware and settings, owned by main.cpp
    Io& io;
    Motor& motor;
    Counter& counter;
    const PublicSettings& pub;
    AdvancedSettings& adv;
    TeachSettings& teach;
    ConfigStorage& store;

    // Parts
    SafetyMonitor safety;
    SafetyActions safetyActions;
    GateCommands commands;
    GateOutputs outputs;
    SpeedRamp ramp;
    MoveTrace trace;
    TeachSequence teachSeq;

    // Limit switches
    bool lsoActivated = false;
    bool lsoActivatedLatched = false;
    bool lscActivated = false;
    bool lscActivatedLatched = false;

    // Pedestrian
    bool pedestrianMode = false;
    bool atPedestrianPosition = false;

    // Motor and position
    bool moving = false;
    int64_t position = 0;                     ///< pulses, 0 = closed
    float rpm = 0.0f;
    int64_t lastUpdateMs = 0;
    bool actualDirectionOpening = false;
    MainLoopStep lastMotorSpeedStep = MainLoopStep::Teach;
    int64_t safetyZoneStartMs = -1;           ///< -1 = not running
    bool loggedLowSpeedClosing = false;
    bool loggedLowSpeedOpening = false;

    // State machine
    MainLoopStep step = MainLoopStep::Teach;
    bool completeOpen = false;
    bool completeClose = false;
    bool stopCountSaved = true;
    bool justClosed = false;
    int torqueRetryCounter = 0;
    bool closeAfterTeach = false;
    bool restartRequested = false;
    bool skipHomingOnNextEntry = false;

    // Timing (ms since boot)
    int64_t lastStartMs = 0;
    int64_t lastStopMs = 0;
    AutoCloseTimer autoCloseTimer;
    int64_t simOpenClosedSinceMs = -1;        ///< -1 = not set

    // Task plumbing
    QueueHandle_t commandQueue = nullptr;
    TaskHandle_t task = nullptr;
    mutable std::mutex statusMu;
    GateStatus status;
    std::function<void()> onCompleteClose;
    std::function<void()> onEntryPoint;   ///< set by main.cpp: wifi reads the Wi-Fi switch
};

#endif // APP_CONTROLLER_GATE_CONTROLLER_H
