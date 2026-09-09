#include "sound_mind/studio/theme.h"

#include <QPixmap>

namespace sound_mind::studio {

namespace {

// clang-format off
const char* const kStyleSheet = R"(
/* Sound Mind Studio brand palette: #DD4B00 (deep orange) to #FEC100
 * (amber gold), on a dark ground - carried over from the legacy
 * documentation's own extra.css (docs/stylesheets/ in the legacy repo).
 * See theme.h's docs for why this is one fixed dark-first look, not a
 * light/dark toggle. */

QMainWindow, QWidget {
    background-color: #1e1e24;
    color: #f0e6d8;
}

QToolTip {
    background-color: #26262e;
    color: #f0e6d8;
    border: 1px solid #FEC100;
}

/* Top menu bar - the header-gradient equivalent of the legacy docs'
 * .md-header. */
QMenuBar {
    background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #DD4B00, stop:1 #FEC100);
    color: #ffffff;
    border: none;
}
QMenuBar::item {
    background: transparent;
    padding: 4px 10px;
}
QMenuBar::item:selected {
    background-color: rgba(255, 255, 255, 45);
}

QMenu {
    background-color: #26262e;
    color: #f0e6d8;
    border: 1px solid #3a3a44;
}
QMenu::item {
    padding: 4px 24px 4px 12px;
}
QMenu::item:selected {
    background-color: #DD4B00;
    color: #ffffff;
}
QMenu::separator {
    height: 1px;
    background-color: #3a3a44;
    margin: 4px 0;
}

/* Transport toolbar - the tabs-bar-gradient equivalent of the legacy
 * docs' .md-tabs. */
QToolBar {
    background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #b53c00, stop:1 #d4a000);
    border: none;
    spacing: 4px;
    padding: 2px;
}
QToolBar QToolButton {
    color: #ffffff;
    background: transparent;
    border: none;
    padding: 4px 10px;
    border-radius: 3px;
}
QToolBar QToolButton:hover {
    background-color: rgba(255, 255, 255, 45);
}
QToolBar QToolButton:checked {
    background-color: #1a0f00;
    color: #FEC100;
}

QStatusBar {
    background-color: #16161a;
    color: #d8cfc0;
    border-top: 1px solid #3a3a44;
}

QPushButton {
    background-color: #2a2a33;
    color: #f0e6d8;
    border: 1px solid #3a3a44;
    border-radius: 4px;
    padding: 5px 14px;
}
QPushButton:hover {
    border-color: #FEC100;
}
QPushButton:pressed {
    background-color: #DD4B00;
    border-color: #DD4B00;
    color: #ffffff;
}
QPushButton:disabled {
    color: #6a6a72;
    border-color: #2a2a33;
}
QPushButton:flat {
    background: transparent;
    border: none;
    color: #e06200;
    padding: 3px 8px;
}
QPushButton:flat:hover {
    color: #FEC100;
    text-decoration: underline;
}

QLabel {
    background: transparent;
    color: #f0e6d8;
}

QLineEdit, QComboBox, QSpinBox, QDoubleSpinBox {
    background-color: #26262e;
    color: #f0e6d8;
    border: 1px solid #3a3a44;
    border-radius: 3px;
    padding: 3px 6px;
    selection-background-color: #DD4B00;
}
QLineEdit:focus, QComboBox:focus, QSpinBox:focus, QDoubleSpinBox:focus {
    border-color: #FEC100;
}

/* Forward-looking - no QDockWidget exists yet (arrives with the Layers
 * Panel milestone, v0.Y.13.1), but styling it now costs nothing and needs
 * no revisiting once it does. */
QDockWidget {
    color: #f0e6d8;
    titlebar-close-icon: none;
}
QDockWidget::title {
    background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #b53c00, stop:1 #d4a000);
    color: #ffffff;
    padding: 4px 6px;
}

QScrollBar:vertical, QScrollBar:horizontal {
    background: #1e1e24;
}
QScrollBar::handle {
    background: #3a3a44;
    border-radius: 4px;
}
QScrollBar::handle:hover {
    background: #FEC100;
}
)";
// clang-format on

}  // namespace

QString studioStyleSheet() { return QString::fromUtf8(kStyleSheet); }

QIcon studioWindowIcon() { return QIcon(QStringLiteral(":/ChooseAgainLarge.png")); }

}  // namespace sound_mind::studio
