#include "app_controller/teach.h"

#include "app_controller/gate_controller.h"
#include "config/config_storage.h"
#include "config/teach_settings.h"
#include "esp_log.h"
#include "hal_io/hal_io.h"
#include "hal_motor/hal_motor.h"
#include "motor_control/time_util.h"

static const char* TAG = "teach";

TeachSequence::TeachSequence(GateController& gate) : gate(gate)
{
}

TeachPhase TeachSequence::getPhase() const
{
    return phase;
}

void TeachSequence::run()
{
    GateController& g = gate;

    phase = TeachPhase::CloseToLimit;
    g.lsoActivated = false;
    g.lscActivated = false;

    // Already taught: skip the teach steps.
    if (g.teach.getPosO() > 0) {
        phase = TeachPhase::Done;
        ESP_LOGI(TAG, "Teach already completed. Skipping teach steps.");
        reestablishGatePosition();
        return;
    }

    int64_t lastPrintCloseMs = 0;
    int64_t lastPrintOpenMs = 0;

    while (phase != TeachPhase::Done) {
        g.safety.pollPhotocell();
        g.safety.pollSafetyEdge();
        g.safety.pollTorqueLimit();

        // Any safety triggered: stop and wait until all are OK again.
        if (!g.safety.allOk()) {
            g.stopMotor();
            while (!g.safety.allOk()) {
                g.publishStatus();   // new: keep the Modbus status current
                sleepMs(kPollIntervalMs);
                g.pollSafetyInputs();
            }
            continue;
        }

        switch (phase) {
        case TeachPhase::CloseToLimit:   // close to LSC
            while (phase == TeachPhase::CloseToLimit) {
                if (!g.ensureSafetyAndResume(Direction::Close)) {
                    continue;
                }
                g.updatePositionAndRpm(false, true);
                g.isLscActivated();

                if (nowMs() - lastPrintCloseMs >= 1000) {
                    ESP_LOGD(TAG, "[TEACH] Closing: counterTotal=%lld", static_cast<long long>(g.position));
                    lastPrintCloseMs = nowMs();
                }

                if (g.lscActivated) {
                    g.stopMotor();
                    phase = TeachPhase::OpenToLimit;
                    g.position = 0;
                    g.lscActivated = false;
                    break;
                }

                if (!g.lscActivated && !g.moving) {
                    g.startMotor(Direction::Close);
                }
                g.publishStatus();   // new: keep the Modbus status current
                sleepMs(kPollIntervalMs);
            }
            break;

        case TeachPhase::OpenToLimit:    // open to LSO
            while (phase == TeachPhase::OpenToLimit) {
                if (!g.ensureSafetyAndResume(Direction::Open)) {
                    continue;
                }
                g.startMotor(Direction::Open);
                g.updatePositionAndRpm(false, true);
                g.isLsoActivated();

                if (nowMs() - lastPrintOpenMs >= 1000) {
                    ESP_LOGD(TAG, "[TEACH] Opening: counterTotal=%lld", static_cast<long long>(g.position));
                    lastPrintOpenMs = nowMs();
                }

                if (g.lsoActivated) {
                    g.stopMotor();
                    phase = TeachPhase::SavePosition;
                    g.lsoActivated = false;
                    break;
                }
                g.publishStatus();   // new: keep the Modbus status current
                sleepMs(kPollIntervalMs);
            }
            break;

        case TeachPhase::SavePosition:   // save the open position
            g.teach.setPosO(g.position);
            g.teach.setPosA(g.position);
            g.store.saveTeach(g.teach);
            ESP_LOGI(TAG, "Teaching complete. POS_O = %lld", static_cast<long long>(g.teach.getPosO()));
            phase = TeachPhase::Done;
            break;

        case TeachPhase::Done:
            break;
        }
    }

    // After teach, close for normal operation.
    g.closeAfterTeach = true;
    g.setMainLoopStep(MainLoopStep::Closing, "Teach completed");
}

void TeachSequence::reestablishGatePosition()
{
    GateController& g = gate;

    const bool atOpen = g.io.isHigh(Input::Lso);
    const bool atClose = g.io.isHigh(Input::Lsc);

    if (atOpen) {
        g.position = g.teach.getPosO();
        ESP_LOGI(TAG, "Gate detected at open limit. Setting counterTotal = POS_O.");
        g.setMainLoopStep(MainLoopStep::Closing, "Closing after power-up");
    } else if (atClose) {
        g.position = 0;
        ESP_LOGI(TAG, "Gate detected at close limit. Setting counterTotal = 0.");
        g.setMainLoopStep(MainLoopStep::ClosingNormalStop, "Position close after power-up");
        g.closeAfterTeach = true;
    } else {
        // Unknown position: close to LSC with all safeties active (blocks until there).
        ESP_LOGI(TAG, "Gate position unknown. Closing to LSC for safe re-homing.");
        while (!g.isLscActivated()) {
            if (!g.ensureSafetyAndResume(Direction::Close)) {
                continue;
            }
            if (!g.isLscActivated() && !g.moving) {
                g.startMotor(Direction::Close);
            }
            g.publishStatus();   // new: keep the Modbus status current
            sleepMs(kPollIntervalMs);
        }
        g.stopMotor();
        g.position = 0;
        ESP_LOGI(TAG, "Homing complete. Gate at close limit.");
        g.closeAfterTeach = true;
        g.setMainLoopStep(MainLoopStep::ClosingNormalStop, "Homing complete, at close limit");
    }
}

void TeachSequence::checkResetButton()
{
    GateController& g = gate;

    if (!g.io.isHigh(Input::Reset)) {
        g.store.clearTeach();
        g.teach.setPosA(0);
        g.teach.setPosO(0);
    }
}
