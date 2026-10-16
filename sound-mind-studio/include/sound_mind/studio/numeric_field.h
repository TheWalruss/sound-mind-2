#pragma once

#include <optional>
#include <vector>

#include <QComboBox>
#include <QPointer>
#include <QString>
#include <QWidget>

#include "sound_mind/core/project_settings.h"

class QLabel;
class QLineEdit;
class QSlider;

namespace sound_mind::studio {

/**
 * @brief How an intensity field shows its level - the dB/value toggle.
 *
 * Values are always stored, emitted and set in dBFS; only the display
 * (text box and unit label) changes. `Value` shows
 * `sound_mind::codec::dbfsToIntensityValue()` - 0 at the -96 dBFS noise
 * floor, 1 at 0 dBFS full scale - the same scale the canvas's own
 * brightness uses. Since that mapping is linear in dB, the slider is the
 * same in both units.
 */
enum class IntensityUnit {
    Decibels,
    Value,
};

/**
 * @brief Parses a number typed by a person, leniently.
 *
 * Surrounding whitespace and any leading/trailing non-numeric text (a unit
 * such as " dB" or "Hz ") are ignored. Either `.` or `,` is accepted as the
 * decimal mark, whatever the project's own setting, but at most one mark
 * may appear. A single leading `+`/`-` is allowed. Leading and trailing
 * zeros are fine ("007.50" is 7.5).
 *
 * @param text The raw text.
 * @return The number, or `std::nullopt` if the text isn't one number
 *         (e.g. "1.2.3", "1 2", "-", or empty).
 */
[[nodiscard]] std::optional<double> parseLenientNumber(const QString& text);

/**
 * @brief Formats `value` for display: rounded to `decimals` places, with
 *        trailing zeros (and a dangling decimal mark) dropped, using
 *        `separator` as the decimal mark. "-0" is shown as "0".
 * @param value The number.
 * @param decimals Maximum number of decimal places.
 * @param separator The decimal mark to use.
 * @return The formatted text.
 */
[[nodiscard]] QString formatNumber(double value, int decimals, sound_mind::core::DecimalSeparator separator);

/**
 * @brief The shared core of NumericField and IntegerField: a horizontal
 *        slider spanning the allowed range plus a lenient text field for an
 *        exact value, replacing up/down spin-box arrows.
 *
 * **Values apply on commit, not per keystroke or per pixel dragged.**
 * - Typing: the value is parsed (parseLenientNumber()), clamped, rounded
 *   and written back in normalized form only when the text field finishes
 *   editing (Enter, or focus moving elsewhere). Text that isn't a number
 *   reverts to the current value.
 * - Dragging the slider previews the value in the text field live, but
 *   applies it only on release.
 * - A slider step that isn't a drag (arrow keys, Page Up/Down, clicking the
 *   groove) applies immediately.
 * - `setValue()` behaves like `QDoubleSpinBox::setValue()`: it emits the
 *   value-changed signal when the value actually changes, and a
 *   `QSignalBlocker` on the field silences it.
 *
 * The decimal mark shown follows the application-wide setting
 * (setDecimalSeparator(), set from the open project's own
 * `ProjectSettings::decimalSeparator`); changing it reformats every open
 * field immediately.
 *
 * @note GUI thread only.
 */
class NumericFieldBase : public QWidget {
    Q_OBJECT

public:
    /// @brief Sets the decimal mark every field shows, and reformats every
    ///        existing field.
    /// @param separator The mark to use.
    static void setDecimalSeparator(sound_mind::core::DecimalSeparator separator);

    /// @brief The decimal mark fields currently show.
    /// @return The current separator; Period until set otherwise.
    [[nodiscard]] static sound_mind::core::DecimalSeparator decimalSeparator() noexcept;

    /// @brief Sets a unit label shown after the text field (e.g. " dB").
    /// @param suffix The label text; empty hides it.
    void setSuffix(const QString& suffix);

    /// @brief The unit label - see setSuffix().
    /// @return The current suffix.
    [[nodiscard]] QString suffix() const;

    /// @brief Sets a label shown before the slider (e.g. "Step 3: ").
    /// @param prefix The label text; empty hides it.
    void setPrefix(const QString& prefix);

    /// @brief The leading label - see setPrefix().
    /// @return The current prefix.
    [[nodiscard]] QString prefix() const;

    /// @brief The text field - exposed for tests and focus handling.
    /// @return The field's own line edit.
    [[nodiscard]] QLineEdit* lineEdit() const noexcept { return lineEdit_; }

    /// @brief The slider - exposed for tests.
    /// @return The field's own slider.
    [[nodiscard]] QSlider* slider() const noexcept { return slider_; }

    /// @brief Shows this field's dBFS value in `unit` - see IntensityUnit.
    ///        Only meaningful for a field holding an absolute dBFS level.
    ///        Changes the display only; never emits.
    /// @param unit The unit to show.
    void setIntensityUnit(IntensityUnit unit);

    /// @brief The unit currently shown - see setIntensityUnit().
    /// @return `Decibels` unless changed.
    [[nodiscard]] IntensityUnit intensityUnit() const noexcept { return intensityUnit_; }

    /// @brief Whether the slider maps position to value logarithmically
    ///        (equal slider distance = equal ratio), for wide positive
    ///        ranges such as Hz. Ignored while the minimum isn't positive.
    /// @param logarithmic `true` for a logarithmic slider.
    void setLogarithmicSlider(bool logarithmic);

    /**
     * @brief Narrows the slider to `[minimum, maximum]` without narrowing
     *        what can be typed (`v0.1.40.8`): the slider covers the useful
     *        range finely, while the text field still accepts anything in
     *        the field's own range. A value beyond the slider range pins the
     *        slider at that end. The slider range is clamped into the
     *        field's range, now and whenever that changes.
     * @param minimum The slider's low end.
     * @param maximum The slider's high end.
     */
    void setSliderRange(double minimum, double maximum);

    /// @brief Goes back to a slider spanning the field's whole range.
    void clearSliderRange();

    /// @brief The slider's low end - the field's minimum unless narrowed.
    /// @return The value at the slider's start.
    [[nodiscard]] double sliderMinimum() const noexcept;

    /// @brief The slider's high end - the field's maximum unless narrowed.
    /// @return The value at the slider's end.
    [[nodiscard]] double sliderMaximum() const noexcept;

protected:
    /// @brief Builds the slider/text/label layout and registers this field
    ///        for decimal-separator changes.
    /// @param parent The parent widget, if any.
    explicit NumericFieldBase(QWidget* parent);
    ~NumericFieldBase() override;

    /// @brief Sets the allowed range; the current value is clamped into it.
    /// @param minimum The lowest allowed value.
    /// @param maximum The highest allowed value (raised to `minimum` if lower).
    void setRangeInternal(double minimum, double maximum);
    /// @brief Sets how many decimal places values are rounded to.
    /// @param decimals Decimal places; negative is treated as 0.
    void setDecimalsInternal(int decimals);
    /// @brief Sets how far one slider arrow-key step moves the value.
    /// @param step The step, in value units (at least one slider position).
    void setSingleStepInternal(double step);
    /// @brief Stores `value` (clamped and rounded) and refreshes the text
    ///        and slider, without emitting.
    /// @param value The new value.
    /// @return `true` if the stored value changed.
    bool setValueInternal(double value);

    /// @return The lowest allowed value.
    [[nodiscard]] double minimumInternal() const noexcept { return minimum_; }
    /// @return The highest allowed value.
    [[nodiscard]] double maximumInternal() const noexcept { return maximum_; }
    /// @return The current value.
    [[nodiscard]] double valueInternal() const noexcept { return value_; }
    /// @return The number of decimal places values are rounded to.
    [[nodiscard]] int decimalsInternal() const noexcept { return decimals_; }

    /// @brief Emits the subclass's own typed value-changed signal.
    virtual void emitValueChanged() = 0;

private:
    void commitText();
    [[nodiscard]] double toDisplay(double value) const;
    [[nodiscard]] double fromDisplay(double displayed) const;
    [[nodiscard]] int displayDecimals() const noexcept;
    void commitValue(double value);
    void refreshText();
    void refreshSlider();
    [[nodiscard]] double clampAndRound(double value) const;
    [[nodiscard]] double sliderPositionToValue(int position) const;
    [[nodiscard]] int valueToSliderPosition(double value) const;
    [[nodiscard]] bool usesLogarithmicSlider() const noexcept;

    QSlider* slider_ = nullptr;
    QLineEdit* lineEdit_ = nullptr;
    QLabel* prefixLabel_ = nullptr;
    QLabel* suffixLabel_ = nullptr;
    double minimum_ = 0.0;
    double maximum_ = 99.99;
    double value_ = 0.0;
    int decimals_ = 2;
    bool logarithmic_ = false;
    /// @brief See setSliderRange(); unset means the whole field range.
    std::optional<std::pair<double, double>> sliderRange_;
    /// @brief The last setSingleStepInternal() step, re-applied when the
    ///        slider's span changes.
    double singleStep_ = 1.0;
    IntensityUnit intensityUnit_ = IntensityUnit::Decibels;
    /// @brief The suffix set via setSuffix(), restored when switching back
    ///        to `Decibels`.
    QString nativeSuffix_;
};

/**
 * @brief A decimal NumericFieldBase - the replacement for `QDoubleSpinBox`.
 *        Defaults match `QDoubleSpinBox`'s: range [0, 99.99], 2 decimals.
 */
class NumericField : public NumericFieldBase {
    Q_OBJECT

public:
    /// @param parent The parent widget, if any.
    explicit NumericField(QWidget* parent = nullptr);

    /// @brief Sets the allowed range; the current value is clamped into it.
    /// @param minimum The lowest allowed value.
    /// @param maximum The highest allowed value.
    void setRange(double minimum, double maximum);
    /// @brief Sets the number of decimal places values are rounded to.
    /// @param decimals Decimal places.
    void setDecimals(int decimals);
    /// @brief Sets how far one slider arrow-key step moves the value.
    /// @param step The step, in value units.
    void setSingleStep(double step);
    /// @brief Sets the value (clamped and rounded); emits valueChanged()
    ///        if it changed.
    /// @param value The new value.
    void setValue(double value);

    /// @return The current value.
    [[nodiscard]] double value() const noexcept { return valueInternal(); }
    /// @return The lowest allowed value.
    [[nodiscard]] double minimum() const noexcept { return minimumInternal(); }
    /// @return The highest allowed value.
    [[nodiscard]] double maximum() const noexcept { return maximumInternal(); }
    /// @return The number of decimal places values are rounded to.
    [[nodiscard]] int decimals() const noexcept { return decimalsInternal(); }

signals:
    /// @brief Emitted when the value changes - on commit, slider release,
    ///        a non-drag slider step, or a changing setValue().
    /// @param value The new value.
    void valueChanged(double value);

protected:
    void emitValueChanged() override;
};

/**
 * @brief A whole-number NumericFieldBase - the replacement for `QSpinBox`.
 *        Typed decimals are rounded to the nearest integer. Defaults match
 *        `QSpinBox`'s: range [0, 99].
 */
class IntegerField : public NumericFieldBase {
    Q_OBJECT

public:
    /// @param parent The parent widget, if any.
    explicit IntegerField(QWidget* parent = nullptr);

    /// @brief Sets the allowed range; the current value is clamped into it.
    /// @param minimum The lowest allowed value.
    /// @param maximum The highest allowed value.
    void setRange(int minimum, int maximum);
    /// @brief Sets how far one slider arrow-key step moves the value.
    /// @param step The step.
    void setSingleStep(int step);
    /// @brief Sets the value (clamped); emits valueChanged() if it changed.
    /// @param value The new value.
    void setValue(int value);

    /// @return The current value.
    [[nodiscard]] int value() const noexcept;
    /// @return The lowest allowed value.
    [[nodiscard]] int minimum() const noexcept;
    /// @return The highest allowed value.
    [[nodiscard]] int maximum() const noexcept;

signals:
    /// @brief Emitted when the value changes - see NumericField::valueChanged().
    /// @param value The new value.
    void valueChanged(int value);

protected:
    void emitValueChanged() override;
};

/**
 * @brief The dB/value toggle for one panel or logical group of intensity
 *        fields: a small combo ("dB" / "value", object name
 *        `intensityUnitCombo`) that applies its choice to every field
 *        added to it. Session-only - not saved.
 * @note GUI thread only.
 */
class IntensityUnitSelector : public QComboBox {
    Q_OBJECT

public:
    /// @param parent The parent widget, if any.
    explicit IntensityUnitSelector(QWidget* parent = nullptr);

    /// @brief Adds `field` to this group, applying the current unit to it.
    /// @param field A field holding an absolute dBFS level.
    void addField(NumericFieldBase* field);

    /// @brief Selects `unit` and applies it to every field in the group.
    /// @param unit The unit to show.
    void setUnit(IntensityUnit unit);

    /// @return The currently selected unit.
    [[nodiscard]] IntensityUnit unit() const;

private:
    void applyToFields();

    std::vector<QPointer<NumericFieldBase>> fields_;
};

}  // namespace sound_mind::studio
