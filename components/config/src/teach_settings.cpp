#include "config/teach_settings.h"

// Copy one value under the mutex.
template <typename T>
T TeachSettings::read(const T& field) const
{
    mu.lock();
    const T copy = field;
    mu.unlock();
    return copy;
}

// Set one value under the mutex.
template <typename T>
void TeachSettings::write(T& field, const T& value)
{
    mu.lock();
    field = value;
    mu.unlock();
}

// Getters and setters: each holds the mutex only while copying one value.
TeachValues TeachSettings::getValues() const
{
    return read(values);
}

void TeachSettings::setValues(const TeachValues& values)
{
    write(this->values, values);
}

int64_t TeachSettings::getPosO() const
{
    return read(values.posO);
}

void TeachSettings::setPosO(int64_t posO)
{
    write(values.posO, posO);
}

int64_t TeachSettings::getPosA() const
{
    return read(values.posA);
}

void TeachSettings::setPosA(int64_t posA)
{
    write(values.posA, posA);
}
