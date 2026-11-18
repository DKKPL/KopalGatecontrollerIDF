#include "config/advanced_settings.h"

#include "config/teach_settings.h"

AdvancedSettings::AdvancedSettings()
{
    applyFactoryDefaults(values.parameterSet, values);
}

void AdvancedSettings::applyDefaults(ParameterSet set)
{
    mu.lock();
    applyFactoryDefaults(set, values);
    mu.unlock();
}

void AdvancedSettings::resetToDefaults()
{
    AdvancedValues fresh;
    applyFactoryDefaults(fresh.parameterSet, fresh);
    mu.lock();
    values = fresh;
    mu.unlock();
}
