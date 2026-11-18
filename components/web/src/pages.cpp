#include "web/pages.h"

#include <cctype>
#include <cerrno>
#include <cinttypes>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <type_traits>
#include <variant>

#include "config/advanced_settings.h"
#include "config/factory_defaults.h"
#include "config/public_settings.h"
#include "config/teach_settings.h"
#include "esp_log.h"
#include "motor_control/move_trace.h"

static const char* TAG = "web";

static constexpr const char* kPublicCss = "body{font-family:Arial,sans-serif;background-color:#f4f4f9;margin:0;padding:0;display:flex;justify-content:center;align-items:flex-start;min-height:100vh;overflow-y:auto}.container{width:100%;max-width:600px;padding:20px;background-color:white;border-radius:8px;box-shadow:0 4px 8px rgba(0,0,0,0.1);margin-top:20px;margin-bottom:20px}h2{text-align:center;margin-bottom:20px}form{display:flex;flex-direction:column}label{margin-bottom:5px;font-size:16px}input{padding:10px;margin:10px 0;font-size:16px;border-radius:4px;border:1px solid #ddd}input[type=submit]{background-color:#4CAF50;color:white;border:none;cursor:pointer}input[type=submit]:hover{background-color:#45a049}p{text-align:center}input:invalid+span::after{content:'Invalid';padding-left:5px}input:valid+span::after{content:'';padding-left:5px}";

static constexpr const char* kAdvancedCss = "body{font-family:Arial;background:#f4f4f9;margin:0;padding:0;display:flex;justify-content:center;align-items:flex-start;min-height:100vh;overflow-y:auto}.container{width:100%;max-width:600px;padding:20px;background:#fff;border-radius:8px;box-shadow:0 0 10px rgba(0,0,0,0.1);margin-top:20px;margin-bottom:20px}input{width:80%;padding:8px;margin:8px 0;font-size:14px}label{display:block;margin-top:10px}input[type=submit]{background-color:#4CAF50;color:white;border:none}h2{text-align:center}input:invalid+span::after{content:'Invalid';padding-left:5px}input:valid+span::after{content:'';padding-left:5px}";

// Rounds "Gate passage" to 2 decimals and every number field to the decimals of its step.
static constexpr const char* kAppJs = R"js(window.addEventListener('DOMContentLoaded',()=>{const g=document.getElementById('GatePassage');if(g){const m=g.textContent.match(/Gate passage:\s*([0-9.eE+-]+)/);if(m){const v=parseFloat(m[1]);if(!isNaN(v)){g.textContent='Gate passage: '+v.toFixed(2);}}}document.querySelectorAll('input[type=number][step]').forEach(i=>{const s=parseFloat(i.step);let d=0;if(!Number.isNaN(s)&&s<1){const p=s.toString().split('.');if(p.length>1){d=p[1].length;}}const v=parseFloat(i.value);if(!Number.isNaN(v)){i.value=v.toFixed(d);}});});)js";

static constexpr const char* kLogoCss = ".logo{display:block;margin:0 auto 10px;max-width:160px;height:auto}";

// Number and bool text like C#'s ToString().
static std::string text(int value)
{
    char buf[16];
    snprintf(buf, sizeof(buf), "%d", value);
    return buf;
}

static std::string text(uint32_t value)
{
    char buf[16];
    snprintf(buf, sizeof(buf), "%" PRIu32, value);
    return buf;
}

static std::string text(int64_t value)
{
    char buf[24];
    snprintf(buf, sizeof(buf), "%" PRId64, value);
    return buf;
}

static std::string text(float value)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "%.7g", static_cast<double>(value));
    return buf;
}

static std::string text(double value)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "%.15g", value);
    return buf;
}

static std::string text(bool value)
{
    return value ? "True" : "False";
}

// The attributes of a required number field.
static std::string num(const char* min, const char* max, const char* step)
{
    return std::string("type='number' min='") + min + "' max='" + max + "' step='" + step + "' required";
}

// Value of one hex digit, -1 when c is not one.
static int hexValue(char c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

// Form-data decoding: '+' is a space, %XX is a byte; a broken % sequence stays as it is.
static std::string urlDecode(const std::string& text)
{
    std::string out;
    out.reserve(text.size());
    for (size_t i = 0; i < text.size(); ++i) {
        const char c = text[i];
        if (c == '+') {
            out += ' ';
        } else if (c == '%' && i + 2 < text.size() && hexValue(text[i + 1]) >= 0 && hexValue(text[i + 2]) >= 0) {
            out += static_cast<char>(hexValue(text[i + 1]) * 16 + hexValue(text[i + 2]));
            i += 2;
        } else {
            out += c;
        }
    }
    return out;
}

// The next "key=value" pair of form data, both decoded (was pair.Split('=') + HttpUtility.UrlDecode).
// pos moves on to the following pair. false when there are no more pairs.
static bool nextPair(const std::string& data, size_t& pos, std::string& key, std::string& val)
{
    if (pos > data.size()) {
        return false;
    }
    size_t end = data.find('&', pos);
    if (end == std::string::npos) {
        end = data.size();
    }
    const std::string pair = data.substr(pos, end - pos);
    pos = end + 1;

    const size_t eq = pair.find('=');
    key = urlDecode(pair.substr(0, eq));
    if (eq == std::string::npos) {
        val.clear();   // no '=': treat as an empty value
        return true;
    }
    const size_t eq2 = pair.find('=', eq + 1);   // C# Split('=') kept only the part up to a second '='
    val = urlDecode(pair.substr(eq + 1, eq2 == std::string::npos ? std::string::npos : eq2 - eq - 1));
    return true;
}

// was int.Parse / float.Parse / double.Parse: the whole text is one number (spaces around are
// allowed, like C#) that fits in T. value is only changed on success.
template <typename T>
static bool parseValue(const std::string& text, T& value)
{
    const char* start = text.c_str();
    char* end = nullptr;
    errno = 0;
    T parsed;
    if constexpr (std::is_same_v<T, int>) {
        const long long whole = strtoll(start, &end, 10);
        if (whole < INT32_MIN || whole > INT32_MAX) {
            return false;
        }
        parsed = static_cast<int>(whole);
    } else if constexpr (std::is_same_v<T, float>) {
        parsed = strtof(start, &end);
    } else {
        parsed = strtod(start, &end);
    }
    if (end == start || errno == ERANGE) {
        return false;
    }
    while (*end == ' ') {
        ++end;
    }
    if (*end != '\0') {
        return false;
    }
    value = parsed;
    return true;
}

// was ParseBoolean: "on", "true" or "1" (any case) is true, anything else false. Never fails.
static bool parseValue(const std::string& text, bool& value)
{
    std::string lower = text;
    for (char& c : lower) {
        c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
    }
    value = lower == "on" || lower == "true" || lower == "1";
    return true;
}

// How a setting is shown on the form.
enum class FieldKind {
    Number,         // number input (was EmitField); min/max/step are its attributes, none when min is null
    NumberInRange,  // like Number, and a value outside min..max is ignored
    Check,          // checkbox, not sent when unchecked (was EmitCheckOn)
    CheckHidden,    // checkbox with a hidden "0" in front, so it is always sent (was EmitCheckHidden)
};

// One setting on a settings page: written by the page writer, read back by the form reader.
template <typename Values>
struct FormField {
    const char* label;   // text in front of the input
    const char* name;    // form field name
    FieldKind kind;
    std::variant<int Values::*, float Values::*, double Values::*, bool Values::*> setting;   // its value in Values
    const char* min = nullptr;    // Number: the attributes of num()
    const char* max = nullptr;
    const char* step = nullptr;
};

// The public page "/", in page order.
static constexpr FormField<PublicValues> kPublicFields[] = {
    { "AutoCloseDelay (s):", "AutoCloseDelay", FieldKind::Number, &PublicValues::autoCloseDelay, "0", "3600", "0" },
    { "PedestrianModeDistance (m):", "PedestrianModeDistance", FieldKind::Number, &PublicValues::pedestrianModeDistance, "0", "6", "0.01" },
    { "PassageLightDelay (s):", "PassageLightDelay", FieldKind::Number, &PublicValues::passageLightDelay, "0", "300", "0" },
    { "BeaconPreWaringTime (s):", "BeaconPreWaringTime", FieldKind::Number, &PublicValues::beaconPreWarningTime, "0", "5", "0" },
    { "ModbusAddress (1-247):", "ModbusAddress", FieldKind::NumberInRange, &PublicValues::modbusAddress, "1", "247", "1" },
    { "Enable captive-portal redirect:", "AutoRedirectEnabled", FieldKind::CheckHidden, &PublicValues::autoRedirectEnabled },
};

// The advanced page "/advanced", in page order after the gate model select.
static constexpr FormField<AdvancedValues> kAdvancedFields[] = {
    { "interrupt SOC DO", "SOC_DO", FieldKind::Check, &AdvancedValues::waitSocDo },
    { "interrupt SO DO", "SO_DO", FieldKind::Check, &AdvancedValues::waitSoDo },
    { "interrupt SOC DC", "SOC_DC", FieldKind::Check, &AdvancedValues::waitSocDc },
    { "interrupt SC DC", "SC_DC", FieldKind::Check, &AdvancedValues::waitScDc },
    { "Resume", "Resume", FieldKind::Check, &AdvancedValues::safetyResume },
    { "BT_AFT_TQ_LMT", "BT_AFT_TQ_LMT", FieldKind::Check, &AdvancedValues::btAftTqLmt },
    { "BT_AFT_SE", "BT_AFT_SE", FieldKind::Check, &AdvancedValues::btAftSe },
    { "Solenoid", "Solenoid", FieldKind::Check, &AdvancedValues::solenoidMagnet },
    { "Use MOTOR_DISABLE pin", "USE_MOTOR_DISABLE", FieldKind::Check, &AdvancedValues::useMotorDisable },
    { "Use Quadrature Encoder", "USE_QUADRATURE_ENCODER", FieldKind::CheckHidden, &AdvancedValues::useQuadratureEncoder },
    { "Invert Encoder Count", "INVERT_ENCODER_COUNT", FieldKind::CheckHidden, &AdvancedValues::invertEncoderCount },
    { "Invert Motor Direction", "INVERT_MOTOR_DIRECTION", FieldKind::CheckHidden, &AdvancedValues::invertMotorDirection },
    { "Use Dual Direction Inputs", "USE_DUAL_DIRECTION_INPUTS", FieldKind::CheckHidden, &AdvancedValues::useDualDirectionInputs },
    { "Simulated open after close (test)", "SIM_OPEN_ENABLED", FieldKind::Check, &AdvancedValues::simOpenEnabled },

    { "BT_TIME (ms)", "BT_TIME", FieldKind::Number, &AdvancedValues::btTime, "0", "2000", "1" },
    { "IdleLockTimeout (s)", "GateIdleLockTimeoutSeconds", FieldKind::Number, &AdvancedValues::gateIdleLockTimeoutSeconds, "0", "500", "1" },
    { "PulsesPerMeter", "PulsesPerMeter", FieldKind::Number, &AdvancedValues::pulsesPerMeter },
    { "EncPulsesPerRotation", "EncoderPulsesPerRotation", FieldKind::Number, &AdvancedValues::encPpr, "1", "100", "1.0" },
    { "OpsLimt (s)", "OpsLimt", FieldKind::Number, &AdvancedValues::opsLimit },
    { "SafetyWaitTime (s)", "SafetyWaitTime", FieldKind::Number, &AdvancedValues::safetyEdgeWaitTime },
    { "SPD_TCH", "SPD_TCH", FieldKind::Number, &AdvancedValues::spdTch, "0", "1", "0.01" },
    { "SPD_EXT_FORCE", "SPD_EXT_FORCE", FieldKind::Number, &AdvancedValues::spdExtForce, "0", "1", "0.01" },
    { "UNLCK_MOVE_TIME (ms)", "UNLCK_MOVE_TIME", FieldKind::Number, &AdvancedValues::unlckMoveTime, "0", "500", "1" },
    { "SPD_HI_C", "SPD_HI_C", FieldKind::Number, &AdvancedValues::spdHiC, "0", "1", "0.01" },
    { "SPD_LO_C", "SPD_LO_C", FieldKind::Number, &AdvancedValues::spdLoC, "0", "1", "0.01" },
    { "SPD_HI_O", "SPD_HI_O", FieldKind::Number, &AdvancedValues::spdHiO, "0", "1", "0.01" },
    { "SPD_LO_O", "SPD_LO_O", FieldKind::Number, &AdvancedValues::spdLoO, "0", "1", "0.01" },
    { "ACC_C", "ACC_C", FieldKind::Number, &AdvancedValues::accC, "0", "10", "0.1" },
    { "DEC_C", "DEC_C", FieldKind::Number, &AdvancedValues::decC, "0", "10", "0.1" },
    { "ACC_O", "ACC_O", FieldKind::Number, &AdvancedValues::accO, "0", "10", "0.1" },
    { "DEC_O", "DEC_O", FieldKind::Number, &AdvancedValues::decO, "0", "10", "0.1" },
    { "TQ_OPS", "TQ_OPS", FieldKind::Number, &AdvancedValues::tqOps, "0", "1", "0.0001" },
    { "TQ_SFTY", "TQ_SFTY", FieldKind::Number, &AdvancedValues::tqSfty, "0", "1", "0.0001" },
    { "APP_C", "APP_C", FieldKind::Number, &AdvancedValues::appC, "0", "1.5", "0.1" },
    { "APP_O", "APP_O", FieldKind::Number, &AdvancedValues::appO, "0", "1.5", "0.1" },
    { "TQ_STFY_ZONE_STRT", "TQ_STFY_ZONE_STRT", FieldKind::Number, &AdvancedValues::tqSftyZoneStrt, "0", "1", "0.01" },
    { "TQ_STFY_ZONE_STOP", "TQ_STFY_ZONE_STOP", FieldKind::Number, &AdvancedValues::tqSftyZoneStop, "-0.5", "0.1", "0.01" },
    { "Simulated open delay (s, 0 = immediate)", "SIM_OPEN_DELAY_S", FieldKind::Number, &AdvancedValues::simOpenDelayS, "0", "900", "1" },
};

// The options of the gate model select, in this order.
static constexpr ParameterSet kParameterSets[] = {
    ParameterSet::Terminus, ParameterSet::Pretorian, ParameterSet::Aluette, ParameterSet::TaurusT2EVA,
};

// One row of a field table as HTML (was EmitField, EmitCheckOn and EmitCheckHidden).
template <typename Values>
static void emitField(const PartSink& s, const FormField<Values>& f, const Values& v)
{
    s("<label>");
    s(f.label);
    s("</label><input ");
    if (f.kind == FieldKind::Check) {
        // An unchecked box is not sent with the form.
        s("name='");
        s(f.name);
        s(v.*std::get<bool Values::*>(f.setting) ? "' type='checkbox' checked/>" : "' type='checkbox'/>");
    } else if (f.kind == FieldKind::CheckHidden) {
        // The hidden "0" makes the form always send the field (0 when unchecked, 0 and 1 when checked; the last one wins).
        s("type='hidden' name='");
        s(f.name);
        s("' value='0'/><input name='");
        s(f.name);
        s(v.*std::get<bool Values::*>(f.setting) ? "' type='checkbox' value='1' checked/>" : "' type='checkbox' value='1'/>");
    } else {
        const std::string value = std::visit([&v](auto setting) { return text(v.*setting); }, f.setting);
        const std::string attrs = f.min != nullptr ? num(f.min, f.max, f.step) : "";
        s("name='");
        s(f.name);
        s("' value='");
        s(value.c_str());
        s("' ");
        s(attrs.c_str());
        s("/>");
        if (!attrs.empty()) {
            s("<span class='validity'></span>");
        }
    }
}

// The gate model select (was EmitSelect with the ParameterSetName names).
static void emitSelect(const PartSink& s, const char* label, const char* name, ParameterSet selected)
{
    s("<label>");
    s(label);
    s("</label><select name='");
    s(name);
    s("'>");
    for (const ParameterSet set : kParameterSets) {
        s("<option value='");
        s(toString(set));
        s(set == selected ? "' selected>" : "'>");
        s(toString(set));
        s("</option>");
    }
    s("</select>");
}

void pages::writePublicPage(const PartSink& s, const PublicSettings& pub, const AdvancedSettings& adv,
                            const TeachSettings& teach)
{
    const double gatePassageMeters = adv.computeGatePassageMeters(teach);
    const PublicValues p = pub.getValues();

    s("<html><head><meta name='viewport' content='width=device-width, initial-scale=1'><style>");
    s(kPublicCss);
    s(kLogoCss);
    s("</style></head><body><div class='container'><img class='logo' src='/logo.jpg' alt='Kopal'/><h2>Edit Public Settings</h2><form method='POST'>");

    for (const FormField<PublicValues>& f : kPublicFields) {
        emitField(s, f, p);
    }

    char passage[32];
    snprintf(passage, sizeof(passage), "%.2f", gatePassageMeters);

    s("<input type='submit' value='Save'/></form><p id='GatePassage'>Gate passage: ");
    s(passage);
    s("</p><p>Cycles: ");
    s(text(adv.getStopCount()).c_str());
    s("</p></div><script>");
    s(kAppJs);
    s("</script></body></html>");
}

void pages::writeAdvancedPage(const PartSink& s, const AdvancedSettings& adv)
{
    const AdvancedValues a = adv.getValues();

    s("<html><head><meta name='viewport' content='width=device-width, initial-scale=1'><style>");
    s(kAdvancedCss);
    s(kLogoCss);
    s("</style></head><body><div class='container'><img class='logo' src='/logo.jpg' alt='Kopal'/><h2>Advanced Settings</h2><form method='POST' action='/advanced'>");

    emitSelect(s, "Parameter set", "Terminus", a.parameterSet);
    for (const FormField<AdvancedValues>& f : kAdvancedFields) {
        emitField(s, f, a);
    }

    s("<input type='submit' value='Save Advanced Settings'/>");
    s("<button type='submit' name='reset' value='1' style='margin-left:10px;background-color:#e74c3c;color:white;'>Reset to Defaults</button>");
    s("<button type='submit' name='reset' value='2' style='margin-left:10px;background-color:#e74c3c;color:white;'>Reset Cycles</button>");
    s("</form>");
    s("<p><a href='/trace' download='trace.csv'>Download last movement trace (CSV)</a></p>");
    s("</div><script>");
    s(kAppJs);
    s("</script></body></html>");
}

// Parse one form value into its setting; a NumberInRange value outside min..max is ignored.
// false when the value is not a valid number (the setting keeps its value).
template <typename Values>
static bool readField(const FormField<Values>& f, const std::string& text, Values& v)
{
    return std::visit([&](auto setting) {
        auto value = v.*setting;
        if (!parseValue(text, value)) {
            return false;
        }
        if (f.kind != FieldKind::NumberInRange || (value >= strtod(f.min, nullptr) && value <= strtod(f.max, nullptr))) {
            v.*setting = value;
        }
        return true;
    }, f.setting);
}

// Read every "name=value" pair of the form whose name is in the table.
// false when a number was invalid (that one keeps its value, the rest is still read).
template <typename Values, size_t N>
static bool readForm(const std::string& formData, const FormField<Values> (&fields)[N], Values& v)
{
    bool allValid = true;
    std::string key;
    std::string val;
    size_t pos = 0;
    while (nextPair(formData, pos, key, val)) {
        for (const FormField<Values>& f : fields) {
            if (key == f.name && !readField(f, val, v)) {
                ESP_LOGW(TAG, "invalid number '%s' for %s", val.c_str(), key.c_str());
                allValid = false;
            }
        }
    }
    return allValid;
}

void pages::writeTrace(const PartSink& s, const AdvancedSettings& adv, const TeachSettings& teach,
                       const MoveTrace& trace)
{
    const AdvancedValues a = adv.getValues();

    std::string line = std::string("# set=") + toString(a.parameterSet)
                     + " ppm=" + text(a.pulsesPerMeter)
                     + " posO=" + text(teach.getPosO())
                     + " appC_pulses=" + text(adv.getAppCPulses()) + " appO_pulses=" + text(adv.getAppOPulses()) + "\r\n";
    s(line.c_str());
    line = "# accC=" + text(a.accC) + " decC=" + text(a.decC) + " hiC=" + text(a.spdHiC) + " loC=" + text(a.spdLoC)
         + " accO=" + text(a.accO) + " decO=" + text(a.decO) + " hiO=" + text(a.spdHiO) + " loO=" + text(a.spdLoO) + "\r\n";
    s(line.c_str());
    line = "# quad=" + text(a.useQuadratureEncoder) + " inv=" + text(a.invertEncoderCount)
         + " dual=" + text(a.useDualDirectionInputs) + " encPpr=" + text(a.encPpr) + "\r\n";
    s(line.c_str());

    trace.write(s);
}
