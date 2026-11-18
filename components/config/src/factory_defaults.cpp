#include "config/factory_defaults.h"

#include "config/advanced_settings.h"
#include "esp_log.h"

static const char* TAG = "factory_defaults";

const char* toString(ParameterSet set)
{
    switch (set) {
    case ParameterSet::Terminus: return "Terminus";
    case ParameterSet::Pretorian: return "Pretorian";
    case ParameterSet::Aluette: return "Aluette";
    case ParameterSet::TaurusT2EVA: return "TaurusT2EVA";
    }
    return "Unknown";
}

// was ResetToTerminusDefaults
static void terminusDefaults(AdvancedValues& v)
{
    ESP_LOGI(TAG, "ResetToTerminusDefaults called");

    v.spdTch = 0.3;
    v.spdExtForce = 1.0;
    v.spdUnlck = 0.12;

    v.pulsesPerMeter = 457.8;
    v.encPpr = 3.0f;

    v.accC = 3.1;
    v.decC = 4.7;
    v.accO = 3.1;
    v.decO = 3.1;

    v.appC = 1.15;
    v.appO = 0.85;

    v.spdHiC = 0.70;
    v.spdHiO = 1.0;
    v.spdLoC = 0.10;
    v.spdLoO = 0.10;

    v.tqOps = 1.0;
    v.tqSfty = 0.6;

    v.useQuadratureEncoder = false;
    v.invertEncoderCount = false;
    v.invertMotorDirection = false;
    v.useDualDirectionInputs = false;

    v.solenoidMagnet = true;
}

// was ResetToPretorianDefaults
static void pretorianDefaults(AdvancedValues& v)
{
    ESP_LOGI(TAG, "ResetToPretorianDefaults called");

    v.spdTch = 0.3;
    v.spdExtForce = 1.0;
    v.spdUnlck = 0.12;

    v.pulsesPerMeter = 717.8;
    v.encPpr = 3.0f;

    v.accC = 4.7;
    v.decC = 4.7;
    v.accO = 3.1;
    v.decO = 3.1;

    v.appC = 1.15;
    v.appO = 0.85;

    v.spdHiC = 0.70;
    v.spdHiO = 0.70;
    v.spdLoC = 0.10;
    v.spdLoO = 0.10;

    v.tqOps = 1.0;
    v.tqSfty = 0.8;

    v.useQuadratureEncoder = false;
    v.invertEncoderCount = false;
    v.invertMotorDirection = false;
    v.useDualDirectionInputs = false;

    v.solenoidMagnet = true;
}

// was ResetToAluetteDefaults
static void aluetteDefaults(AdvancedValues& v)
{
    ESP_LOGI(TAG, "ResetToAluetteDefaults called");

    v.spdTch = 0.7;
    v.spdExtForce = 0.0;
    v.spdUnlck = 0.88;

    // With a 6 PPR encoder: pulsesPerMeter = 1773.4, encPpr = 6.0
    // With a 3 PPR encoder:
    v.pulsesPerMeter = 886.7;
    v.encPpr = 3.0f;

    v.accC = 4.7;
    v.decC = 4.7;
    v.accO = 4.7;
    v.decO = 4.7;

    v.appC = 1.15;
    v.appO = 0.85;

    v.spdHiC = 0.3;
    v.spdHiO = 0.0;
    v.spdLoC = 0.9;
    v.spdLoO = 0.9;

    v.tqOps = 1.0;
    v.tqSfty = 0.8;

    v.useQuadratureEncoder = true;
    v.invertEncoderCount = false;
    v.invertMotorDirection = true;
    v.useDualDirectionInputs = true;

    v.solenoidMagnet = false;
}

// was ResetToTaurusT2EVADefaults
static void taurusT2EvaDefaults(AdvancedValues& v)
{
    ESP_LOGI(TAG, "ResetToTaurusT2EVADefaults called");

    v.spdTch = 0.3;
    v.spdExtForce = 1.0;
    v.spdUnlck = 0.12;

    v.pulsesPerMeter = 717.8;
    v.encPpr = 3.0f;

    v.accC = 4.7;
    v.decC = 4.7;
    v.accO = 4.7;
    v.decO = 4.7;

    v.appC = 1.15;
    v.appO = 0.85;

    v.spdHiC = 0.70;
    v.spdHiO = 0.7;
    v.spdLoC = 0.10;
    v.spdLoO = 0.10;

    v.tqOps = 1.0;
    v.tqSfty = 1.0;

    v.useQuadratureEncoder = false;
    v.invertEncoderCount = false;
    v.invertMotorDirection = false;
    v.useDualDirectionInputs = false;

    v.solenoidMagnet = true;
}

// was ApplyDefaults (the switch)
void applyFactoryDefaults(ParameterSet set, AdvancedValues& values)
{
    switch (set) {
    case ParameterSet::Terminus: terminusDefaults(values); break;
    case ParameterSet::Pretorian: pretorianDefaults(values); break;
    case ParameterSet::Aluette: aluetteDefaults(values); break;
    case ParameterSet::TaurusT2EVA: taurusT2EvaDefaults(values); break;
    }
}
