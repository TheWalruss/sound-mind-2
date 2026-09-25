#include "sound_mind/studio/fill_gradient_dialog.h"

#include <utility>

#include <QDialogButtonBox>
#include <QVBoxLayout>

#include "sound_mind/studio/gradient_editor_widget.h"

namespace sound_mind::studio {

FillGradientDialog::FillGradientDialog(sound_mind::core::Gradient gradient, QWidget* parent) : QDialog(parent) {
    setWindowTitle(tr("Fill Selection"));

    auto* root = new QVBoxLayout(this);

    editor_ = new GradientEditorWidget(this);
    editor_->setObjectName(QStringLiteral("gradientEditor"));
    editor_->setGradient(std::move(gradient));
    root->addWidget(editor_);

    auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttonBox->setObjectName(QStringLiteral("buttonBox"));
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(buttonBox);
}

const sound_mind::core::Gradient& FillGradientDialog::gradient() const noexcept { return editor_->gradient(); }

}  // namespace sound_mind::studio
