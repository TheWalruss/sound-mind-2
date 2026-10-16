#include "sound_mind/studio/numeric_field.h"

#include "sound_mind/codec/color_mapping.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QSignalBlocker>
#include <QSlider>

namespace sound_mind::studio {

namespace {

/// @brief Slider resolution - positions 0..kSliderSteps across the range.
constexpr int kSliderSteps = 1000;

[[nodiscard]] bool isNumberChar(QChar c) {
    return c.isDigit() || c == QLatin1Char('.') || c == QLatin1Char(',') || c == QLatin1Char('+') ||
           c == QLatin1Char('-');
}

sound_mind::core::DecimalSeparator& currentSeparator() {
    static sound_mind::core::DecimalSeparator separator = sound_mind::core::DecimalSeparator::Period;
    return separator;
}

std::vector<NumericFieldBase*>& liveFields() {
    static std::vector<NumericFieldBase*> fields;
    return fields;
}

}  // namespace

std::optional<double> parseLenientNumber(const QString& text) {
    const QString trimmed = text.trimmed();
    qsizetype first = 0;
    while (first < trimmed.size() && !isNumberChar(trimmed[first])) {
        ++first;
    }
    qsizetype last = trimmed.size();
    while (last > first && !isNumberChar(trimmed[last - 1])) {
        --last;
    }
    QString body = trimmed.mid(first, last - first);
    if (body.isEmpty()) {
        return std::nullopt;
    }

    bool negative = false;
    if (body.front() == QLatin1Char('+') || body.front() == QLatin1Char('-')) {
        negative = body.front() == QLatin1Char('-');
        body.remove(0, 1);
    }

    int marks = 0;
    int digits = 0;
    for (const QChar c : body) {
        if (c.isDigit()) {
            ++digits;
        } else if (c == QLatin1Char('.') || c == QLatin1Char(',')) {
            ++marks;
        } else {
            return std::nullopt;  // a space, a second sign, a stray letter...
        }
    }
    if (digits == 0 || marks > 1) {
        return std::nullopt;
    }

    body.replace(QLatin1Char(','), QLatin1Char('.'));
    bool ok = false;
    const double magnitude = QLocale::c().toDouble(body, &ok);
    if (!ok) {
        return std::nullopt;
    }
    return negative ? -magnitude : magnitude;
}

QString formatNumber(double value, int decimals, sound_mind::core::DecimalSeparator separator) {
    QString text = QString::number(value, 'f', std::max(decimals, 0));
    if (text.contains(QLatin1Char('.'))) {
        while (text.endsWith(QLatin1Char('0'))) {
            text.chop(1);
        }
        if (text.endsWith(QLatin1Char('.'))) {
            text.chop(1);
        }
    }
    if (text == QStringLiteral("-0")) {
        text = QStringLiteral("0");
    }
    if (separator == sound_mind::core::DecimalSeparator::Comma) {
        text.replace(QLatin1Char('.'), QLatin1Char(','));
    }
    return text;
}

// --- NumericFieldBase --------------------------------------------------

void NumericFieldBase::setDecimalSeparator(sound_mind::core::DecimalSeparator separator) {
    currentSeparator() = separator;
    for (NumericFieldBase* field : liveFields()) {
        field->refreshText();
    }
}

sound_mind::core::DecimalSeparator NumericFieldBase::decimalSeparator() noexcept { return currentSeparator(); }

NumericFieldBase::NumericFieldBase(QWidget* parent) : QWidget(parent) {
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    prefixLabel_ = new QLabel(this);
    prefixLabel_->hide();
    layout->addWidget(prefixLabel_);

    slider_ = new QSlider(Qt::Horizontal, this);
    slider_->setRange(0, kSliderSteps);
    slider_->setPageStep(kSliderSteps / 10);
    layout->addWidget(slider_, 1);

    lineEdit_ = new QLineEdit(this);
    lineEdit_->setMaximumWidth(80);
    lineEdit_->setAlignment(Qt::AlignRight);
    layout->addWidget(lineEdit_);

    suffixLabel_ = new QLabel(this);
    suffixLabel_->hide();
    layout->addWidget(suffixLabel_);

    connect(lineEdit_, &QLineEdit::editingFinished, this, &NumericFieldBase::commitText);
    connect(slider_, &QSlider::sliderMoved, this, [this](int position) {
        lineEdit_->setText(formatNumber(toDisplay(clampAndRound(sliderPositionToValue(position))), displayDecimals(),
                                        currentSeparator()));
    });
    connect(slider_, &QSlider::sliderReleased, this,
            [this]() { commitValue(sliderPositionToValue(slider_->value())); });
    connect(slider_, &QSlider::valueChanged, this, [this](int position) {
        if (!slider_->isSliderDown()) {
            commitValue(sliderPositionToValue(position));
        }
    });

    liveFields().push_back(this);
    refreshText();
    refreshSlider();
}

NumericFieldBase::~NumericFieldBase() { std::erase(liveFields(), this); }

void NumericFieldBase::setSuffix(const QString& suffix) {
    nativeSuffix_ = suffix.trimmed();
    const QString shown = intensityUnit_ == IntensityUnit::Value ? QString() : nativeSuffix_;
    suffixLabel_->setText(shown);
    suffixLabel_->setVisible(!shown.isEmpty());
}

void NumericFieldBase::setIntensityUnit(IntensityUnit unit) {
    intensityUnit_ = unit;
    setSuffix(nativeSuffix_);
    refreshText();
}

double NumericFieldBase::toDisplay(double value) const {
    return intensityUnit_ == IntensityUnit::Value
               ? static_cast<double>(sound_mind::codec::dbfsToIntensityValue(static_cast<float>(value)))
               : value;
}

double NumericFieldBase::fromDisplay(double displayed) const {
    return intensityUnit_ == IntensityUnit::Value
               ? static_cast<double>(sound_mind::codec::intensityValueToDbfs(static_cast<float>(displayed)))
               : displayed;
}

int NumericFieldBase::displayDecimals() const noexcept {
    // A value of 0..1 spans 96 dB, so 3 places is about 0.1 dB resolution.
    return intensityUnit_ == IntensityUnit::Value ? 3 : decimals_;
}

QString NumericFieldBase::suffix() const { return suffixLabel_->text(); }

void NumericFieldBase::setPrefix(const QString& prefix) {
    prefixLabel_->setText(prefix.trimmed());
    prefixLabel_->setVisible(!prefix.trimmed().isEmpty());
}

QString NumericFieldBase::prefix() const { return prefixLabel_->text(); }

void NumericFieldBase::setLogarithmicSlider(bool logarithmic) {
    logarithmic_ = logarithmic;
    refreshSlider();
}

void NumericFieldBase::setSliderRange(double minimum, double maximum) {
    sliderRange_ = std::pair{minimum, std::max(minimum, maximum)};
    setSingleStepInternal(singleStep_);
    refreshSlider();
}

void NumericFieldBase::clearSliderRange() {
    sliderRange_.reset();
    setSingleStepInternal(singleStep_);
    refreshSlider();
}

double NumericFieldBase::sliderMinimum() const noexcept {
    return sliderRange_.has_value() ? std::clamp(sliderRange_->first, minimum_, maximum_) : minimum_;
}

double NumericFieldBase::sliderMaximum() const noexcept {
    return sliderRange_.has_value() ? std::clamp(sliderRange_->second, sliderMinimum(), maximum_) : maximum_;
}

void NumericFieldBase::setRangeInternal(double minimum, double maximum) {
    minimum_ = minimum;
    maximum_ = std::max(minimum, maximum);
    value_ = clampAndRound(value_);
    refreshText();
    refreshSlider();
}

void NumericFieldBase::setDecimalsInternal(int decimals) {
    decimals_ = std::max(decimals, 0);
    value_ = clampAndRound(value_);
    refreshText();
}

void NumericFieldBase::setSingleStepInternal(double step) {
    singleStep_ = step;
    const double span = sliderMaximum() - sliderMinimum();
    const int positions =
        span > 0.0 ? static_cast<int>(std::lround(step / span * static_cast<double>(kSliderSteps))) : 1;
    const QSignalBlocker blocker(slider_);
    slider_->setSingleStep(std::max(positions, 1));
}

bool NumericFieldBase::setValueInternal(double value) {
    const double clamped = clampAndRound(value);
    const bool changed = clamped != value_;
    value_ = clamped;
    refreshText();
    refreshSlider();
    return changed;
}

void NumericFieldBase::commitText() {
    const auto parsed = parseLenientNumber(lineEdit_->text());
    if (!parsed.has_value()) {
        refreshText();
        return;
    }
    commitValue(fromDisplay(*parsed));
}

void NumericFieldBase::commitValue(double value) {
    if (setValueInternal(value)) {
        emitValueChanged();
    }
}

void NumericFieldBase::refreshText() {
    const QSignalBlocker blocker(lineEdit_);
    lineEdit_->setText(formatNumber(toDisplay(value_), displayDecimals(), currentSeparator()));
}

void NumericFieldBase::refreshSlider() {
    const QSignalBlocker blocker(slider_);
    slider_->setValue(valueToSliderPosition(value_));
}

double NumericFieldBase::clampAndRound(double value) const {
    const double scale = std::pow(10.0, decimals_);
    const double rounded = std::round(value * scale) / scale;
    return std::clamp(rounded, minimum_, maximum_);
}

bool NumericFieldBase::usesLogarithmicSlider() const noexcept {
    return logarithmic_ && sliderMinimum() > 0.0 && sliderMaximum() > sliderMinimum();
}

double NumericFieldBase::sliderPositionToValue(int position) const {
    const double low = sliderMinimum();
    const double high = sliderMaximum();
    const double fraction = static_cast<double>(position) / static_cast<double>(kSliderSteps);
    if (usesLogarithmicSlider()) {
        return low * std::pow(high / low, fraction);
    }
    return low + fraction * (high - low);
}

int NumericFieldBase::valueToSliderPosition(double value) const {
    const double low = sliderMinimum();
    const double high = sliderMaximum();
    if (high <= low) {
        return 0;
    }
    const double fraction = usesLogarithmicSlider() ? std::log(value / low) / std::log(high / low)
                                                    : (value - low) / (high - low);
    return static_cast<int>(std::lround(std::clamp(fraction, 0.0, 1.0) * static_cast<double>(kSliderSteps)));
}

// --- NumericField --------------------------------------------------------

NumericField::NumericField(QWidget* parent) : NumericFieldBase(parent) {
    setRangeInternal(0.0, 99.99);
    setDecimalsInternal(2);
    setSingleStepInternal(1.0);
}

void NumericField::setRange(double minimum, double maximum) { setRangeInternal(minimum, maximum); }

void NumericField::setDecimals(int decimals) { setDecimalsInternal(decimals); }

void NumericField::setSingleStep(double step) { setSingleStepInternal(step); }

void NumericField::setValue(double value) {
    if (setValueInternal(value)) {
        emitValueChanged();
    }
}

void NumericField::emitValueChanged() { emit valueChanged(valueInternal()); }

// --- IntegerField --------------------------------------------------------

IntegerField::IntegerField(QWidget* parent) : NumericFieldBase(parent) {
    setDecimalsInternal(0);
    setRangeInternal(0.0, 99.0);
    setSingleStepInternal(1.0);
}

void IntegerField::setRange(int minimum, int maximum) {
    setRangeInternal(static_cast<double>(minimum), static_cast<double>(maximum));
}

void IntegerField::setSingleStep(int step) { setSingleStepInternal(static_cast<double>(step)); }

void IntegerField::setValue(int value) {
    if (setValueInternal(static_cast<double>(value))) {
        emitValueChanged();
    }
}

int IntegerField::value() const noexcept { return static_cast<int>(std::lround(valueInternal())); }

int IntegerField::minimum() const noexcept { return static_cast<int>(std::lround(minimumInternal())); }

int IntegerField::maximum() const noexcept { return static_cast<int>(std::lround(maximumInternal())); }

void IntegerField::emitValueChanged() { emit valueChanged(value()); }

// --- IntensityUnitSelector -------------------------------------------------

IntensityUnitSelector::IntensityUnitSelector(QWidget* parent) : QComboBox(parent) {
    setObjectName(QStringLiteral("intensityUnitCombo"));
    setToolTip(tr("Show intensity in dB, or as a value from 0 (silence) to 1 (full scale)"));
    addItem(tr("dB"), static_cast<int>(IntensityUnit::Decibels));
    addItem(tr("value"), static_cast<int>(IntensityUnit::Value));
    connect(this, &QComboBox::currentIndexChanged, this, [this]() { applyToFields(); });
}

void IntensityUnitSelector::addField(NumericFieldBase* field) {
    fields_.emplace_back(field);
    field->setIntensityUnit(unit());
}

void IntensityUnitSelector::setUnit(IntensityUnit unit) { setCurrentIndex(findData(static_cast<int>(unit))); }

IntensityUnit IntensityUnitSelector::unit() const { return static_cast<IntensityUnit>(currentData().toInt()); }

void IntensityUnitSelector::applyToFields() {
    for (const auto& field : fields_) {
        if (field != nullptr) {
            field->setIntensityUnit(unit());
        }
    }
}

}  // namespace sound_mind::studio
