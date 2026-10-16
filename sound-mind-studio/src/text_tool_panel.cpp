#include "sound_mind/studio/text_tool_panel.h"

#include <array>
#include <utility>

#include <QLabel>
#include <QCheckBox>
#include <QComboBox>
#include <QFontComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

#include "sound_mind/studio/numeric_field.h"

namespace sound_mind::studio {

namespace {

/// @brief The blend modes text can be written with - the same list a paste
/// offers, since applied text is a paste.
constexpr std::array<std::pair<sound_mind::core::BlendMode, const char*>, 7> kTextBlendModes = {{
    {sound_mind::core::BlendMode::Normal, "Normal"},
    {sound_mind::core::BlendMode::Overwrite, "Overwrite"},
    {sound_mind::core::BlendMode::Multiply, "Multiply"},
    {sound_mind::core::BlendMode::Screen, "Screen"},
    {sound_mind::core::BlendMode::Overlay, "Overlay"},
    {sound_mind::core::BlendMode::Difference, "Difference"},
    {sound_mind::core::BlendMode::Add, "Add"},
}};

}  // namespace

TextToolPanel::TextToolPanel(QWidget* parent) : QDockWidget(tr("Text Tool"), parent) {
    setObjectName(QStringLiteral("textToolPanel"));
    auto* container = new QWidget(this);
    auto* root = new QVBoxLayout(container);
    auto* form = new QFormLayout();

    const TextSettings defaults;
    textEdit_ = new QLineEdit(container);
    textEdit_->setObjectName(QStringLiteral("textEdit"));
    textEdit_->setPlaceholderText(tr("Type the text to write"));
    form->addRow(tr("Text:"), textEdit_);

    fontCombo_ = new QFontComboBox(container);
    fontCombo_->setObjectName(QStringLiteral("textFontCombo"));
    fontCombo_->setCurrentFont(QFont(defaults.fontFamily));
    form->addRow(tr("Font:"), fontCombo_);

    sizeField_ = new IntegerField(container);
    sizeField_->setObjectName(QStringLiteral("textSizeField"));
    sizeField_->setRange(4, 400);
    sizeField_->setValue(defaults.fontSize);
    sizeField_->setToolTip(tr("Letter height, in canvas cells (time columns / frequency rows)"));
    form->addRow(tr("Size:"), sizeField_);

    auto* styleRow = new QHBoxLayout();
    boldCheckBox_ = new QCheckBox(tr("Bold"), container);
    boldCheckBox_->setObjectName(QStringLiteral("textBoldCheckBox"));
    italicCheckBox_ = new QCheckBox(tr("Italic"), container);
    italicCheckBox_->setObjectName(QStringLiteral("textItalicCheckBox"));
    styleRow->addWidget(boldCheckBox_);
    styleRow->addWidget(italicCheckBox_);
    styleRow->addStretch();
    form->addRow(tr("Style:"), styleRow);

    intensityUnit_ = new IntensityUnitSelector(container);
    form->addRow(tr("Intensity unit:"), intensityUnit_);
    leftIntensityField_ = new NumericField(container);
    leftIntensityField_->setObjectName(QStringLiteral("textLeftIntensityField"));
    rightIntensityField_ = new NumericField(container);
    rightIntensityField_->setObjectName(QStringLiteral("textRightIntensityField"));
    for (NumericField* field : {leftIntensityField_, rightIntensityField_}) {
        field->setRange(-96.0, 0.0);
        field->setDecimals(1);
        field->setValue(defaults.leftIntensityDb);
        intensityUnit_->addField(field);
    }
    form->addRow(tr("Left intensity:"), leftIntensityField_);
    form->addRow(tr("Right intensity:"), rightIntensityField_);

    opacityField_ = new NumericField(container);
    opacityField_->setObjectName(QStringLiteral("textOpacityField"));
    opacityField_->setRange(0.0, 1.0);
    opacityField_->setDecimals(2);
    opacityField_->setValue(defaults.opacity);
    form->addRow(tr("Opacity:"), opacityField_);

    blendModeCombo_ = new QComboBox(container);
    blendModeCombo_->setObjectName(QStringLiteral("textBlendModeCombo"));
    for (const auto& [mode, name] : kTextBlendModes) {
        blendModeCombo_->addItem(tr(name), QVariant::fromValue(static_cast<int>(mode)));
    }
    form->addRow(tr("Blend mode:"), blendModeCombo_);

    rotationField_ = new NumericField(container);
    rotationField_->setObjectName(QStringLiteral("textRotationField"));
    rotationField_->setRange(-180.0, 180.0);
    rotationField_->setDecimals(1);
    rotationField_->setValue(defaults.rotationDegrees);
    rotationField_->setToolTip(tr("Rotation in degrees, counterclockwise"));
    form->addRow(tr("Rotation:"), rotationField_);
    root->addLayout(form);

    auto* buttons = new QHBoxLayout();
    applyButton_ = new QPushButton(tr("Apply"), container);
    applyButton_->setObjectName(QStringLiteral("textApplyButton"));
    applyButton_->setToolTip(tr("Write the text onto the layer"));
    resetButton_ = new QPushButton(tr("Reset"), container);
    resetButton_->setObjectName(QStringLiteral("textResetButton"));
    resetButton_->setToolTip(tr("Discard the text box"));
    buttons->addWidget(applyButton_);
    buttons->addWidget(resetButton_);
    root->addLayout(buttons);
    editingPickedLabel_ = new QLabel(
        tr("Editing the picked text - changes apply as you make them. Pick something else, or use the Text tool, "
           "to stop."),
        container);
    editingPickedLabel_->setObjectName(QStringLiteral("textEditingPickedLabel"));
    editingPickedLabel_->setWordWrap(true);
    editingPickedLabel_->hide();
    root->addWidget(editingPickedLabel_);
    root->addStretch();
    setWidget(container);
    setBoxPlaced(false);

    connect(textEdit_, &QLineEdit::textChanged, this, &TextToolPanel::emitSettings);
    connect(fontCombo_, &QFontComboBox::currentFontChanged, this, &TextToolPanel::emitSettings);
    connect(sizeField_, &IntegerField::valueChanged, this, &TextToolPanel::emitSettings);
    connect(boldCheckBox_, &QCheckBox::toggled, this, &TextToolPanel::emitSettings);
    connect(italicCheckBox_, &QCheckBox::toggled, this, &TextToolPanel::emitSettings);
    for (NumericField* field : {leftIntensityField_, rightIntensityField_, opacityField_, rotationField_}) {
        connect(field, &NumericField::valueChanged, this, &TextToolPanel::emitSettings);
    }
    connect(blendModeCombo_, &QComboBox::currentIndexChanged, this, &TextToolPanel::emitSettings);
    connect(applyButton_, &QPushButton::clicked, this, &TextToolPanel::applyRequested);
    connect(resetButton_, &QPushButton::clicked, this, &TextToolPanel::resetRequested);
}

TextSettings TextToolPanel::settings() const {
    TextSettings settings;
    settings.text = textEdit_->text();
    settings.fontFamily = fontCombo_->currentFont().family();
    settings.fontSize = sizeField_->value();
    settings.bold = boldCheckBox_->isChecked();
    settings.italic = italicCheckBox_->isChecked();
    settings.leftIntensityDb = static_cast<float>(leftIntensityField_->value());
    settings.rightIntensityDb = static_cast<float>(rightIntensityField_->value());
    settings.opacity = static_cast<float>(opacityField_->value());
    settings.blendMode = static_cast<sound_mind::core::BlendMode>(blendModeCombo_->currentData().toInt());
    settings.rotationDegrees = rotationField_->value();
    return settings;
}

void TextToolPanel::setSettings(const TextSettings& settings) {
    loading_ = true;
    textEdit_->setText(settings.text);
    fontCombo_->setCurrentFont(QFont(settings.fontFamily));
    sizeField_->setValue(settings.fontSize);
    boldCheckBox_->setChecked(settings.bold);
    italicCheckBox_->setChecked(settings.italic);
    leftIntensityField_->setValue(settings.leftIntensityDb);
    rightIntensityField_->setValue(settings.rightIntensityDb);
    opacityField_->setValue(settings.opacity);
    blendModeCombo_->setCurrentIndex(blendModeCombo_->findData(static_cast<int>(settings.blendMode)));
    rotationField_->setValue(settings.rotationDegrees);
    loading_ = false;
}

void TextToolPanel::setEditingPickedText(bool editing) {
    editingPickedText_ = editing;
    editingPickedLabel_->setVisible(editing);
}

void TextToolPanel::setBoxPlaced(bool placed) {
    applyButton_->setEnabled(placed);
    resetButton_->setEnabled(placed);
}

void TextToolPanel::emitSettings() {
    if (!loading_) {
        emit settingsChanged(settings());
    }
}

}  // namespace sound_mind::studio
