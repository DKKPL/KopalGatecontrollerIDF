#ifndef CONFIG_TEACH_SETTINGS_H
#define CONFIG_TEACH_SETTINGS_H

#include <cstdint>
#include <mutex>

/**
 * @brief The taught positions as plain values; saved as one NVS blob (was the TeachSettings properties).
 */
struct TeachValues {
    int64_t posO = 0;   ///< fully open position in pulses from closed, 0 = not taught (was POS_O)
    int64_t posA = 0;   ///< actual gate position in pulses (was POS_A)
};

/**
 * @brief Taught gate positions (was TeachSettings). Thread safe.
 */
class TeachSettings {
public:
    /**
     * @brief Copy of both positions, taken under one lock (new).
     */
    TeachValues getValues() const;
    /**
     * @brief Replace both positions in one go (new).
     */
    void setValues(const TeachValues& values);

    /**
     * @brief Fully open position in pulses, 0 = not taught (was POS_O).
     */
    int64_t getPosO() const;
    /**
     * @brief Set POS_O.
     */
    void setPosO(int64_t posO);
    /**
     * @brief Actual gate position in pulses (was POS_A).
     */
    int64_t getPosA() const;
    /**
     * @brief Set POS_A.
     */
    void setPosA(int64_t posA);

private:
    /**
     * @brief Copy one value under the mutex.
     */
    template <typename T>
    T read(const T& field) const;

    /**
     * @brief Set one value under the mutex.
     */
    template <typename T>
    void write(T& field, const T& value);

    mutable std::mutex mu;   ///< the gate writes while the web reads
    TeachValues values;      ///< both saved positions
};

#endif // CONFIG_TEACH_SETTINGS_H
