#ifndef CONFIG_FACTORY_DEFAULTS_H
#define CONFIG_FACTORY_DEFAULTS_H

#include <cstdint>

struct AdvancedValues;

/**
 * @brief Gate model; selects the factory defaults (was ParameterSet).
 */
enum class ParameterSet : int32_t {
    Terminus = 0,     ///< was ParameterSet.Terminus
    Pretorian = 1,    ///< was ParameterSet.Pretorian
    Aluette = 2,      ///< was ParameterSet.Aluette
    TaurusT2EVA = 3,  ///< was ParameterSet.TaurusT2EVA
};

/**
 * @brief Name of a gate model, e.g. "Terminus" (was ParameterSet.ToString()).
 */
const char* toString(ParameterSet set);

/**
 * @brief Overwrite the model-dependent values with the factory defaults of a gate model.
 * Other values stay; an unknown model changes nothing (was the ResetTo...Defaults methods).
 */
void applyFactoryDefaults(ParameterSet set, AdvancedValues& values);

#endif // CONFIG_FACTORY_DEFAULTS_H
