#ifndef WEB_PAGES_H
#define WEB_PAGES_H

#include <functional>
#include <string>

class AdvancedSettings;
class MoveTrace;
class PublicSettings;
class TeachSettings;
struct AdvancedValues;
struct PublicValues;

/**
 * @brief Receives a page piece by piece (was the delegate PartSink).
 */
using PartSink = std::function<void(const char* part)>;

/**
 * @brief Writes a whole page into a PartSink (was the delegate PageWriter).
 */
using PageWriter = std::function<void(const PartSink& sink)>;

namespace pages {

/**
 * @brief The public settings page "/" (was WritePublicPage).
 */
void writePublicPage(const PartSink& s, const PublicSettings& pub, const AdvancedSettings& adv,
                     const TeachSettings& teach);

/**
 * @brief The advanced settings page "/advanced" (was WriteAdvancedPage).
 */
void writeAdvancedPage(const PartSink& s, const AdvancedSettings& adv);

/**
 * @brief The "/trace" CSV: three "#" lines with the settings, then the last movement (was WriteTrace).
 */
void writeTrace(const PartSink& s, const AdvancedSettings& adv, const TeachSettings& teach,
                const MoveTrace& trace);

/**
 * @brief Put the values of the public form into v (was the loop in ProcessPublicPost).
 * @return false when a number is invalid; v must not be used then
 */
bool readPublicForm(const std::string& formData, PublicValues& v);

/**
 * @brief Put the values of the advanced form into v: gate model, checkboxes, numbers, and the
 *        model's factory defaults when the model changed (was the middle of ProcessAdvancedPost).
 */
void readAdvancedForm(const std::string& formData, AdvancedValues& v);

} // namespace pages

#endif // WEB_PAGES_H
