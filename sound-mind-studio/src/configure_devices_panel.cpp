#include "sound_mind/studio/configure_devices_panel.h"

#include <QScrollArea>

#include "sound_mind/studio/device_configuration_widget.h"

namespace sound_mind::studio {

ConfigureDevicesPanel::ConfigureDevicesPanel(QWidget* parent) : QDockWidget(tr("Configure Devices"), parent) {
    widget_ = new DeviceConfigurationWidget(this);
    connect(widget_, &DeviceConfigurationWidget::refreshRequested, this, &ConfigureDevicesPanel::refreshRequested);
    connect(widget_, &DeviceConfigurationWidget::inputDeviceChanged, this,
            &ConfigureDevicesPanel::inputDeviceChanged);
    connect(widget_, &DeviceConfigurationWidget::outputDeviceChanged, this,
            &ConfigureDevicesPanel::outputDeviceChanged);
    connect(widget_, &DeviceConfigurationWidget::inputGainPercentChanged, this,
            &ConfigureDevicesPanel::inputGainPercentChanged);
    connect(widget_, &DeviceConfigurationWidget::outputGainPercentChanged, this,
            &ConfigureDevicesPanel::outputGainPercentChanged);
    connect(widget_, &DeviceConfigurationWidget::testInputToggled, this, &ConfigureDevicesPanel::testInputToggled);
    connect(widget_, &DeviceConfigurationWidget::testOutputToggled, this, &ConfigureDevicesPanel::testOutputToggled);

    auto* scrollArea = new QScrollArea(this);
    scrollArea->setWidget(widget_);
    scrollArea->setWidgetResizable(true);
    setWidget(scrollArea);
}

void ConfigureDevicesPanel::setInputDevices(const QStringList& deviceNames) { widget_->setInputDevices(deviceNames); }

void ConfigureDevicesPanel::setOutputDevices(const QStringList& deviceNames) {
    widget_->setOutputDevices(deviceNames);
}

void ConfigureDevicesPanel::setSelectedInputDevice(const QString& deviceName) {
    widget_->setSelectedInputDevice(deviceName);
}

void ConfigureDevicesPanel::setSelectedOutputDevice(const QString& deviceName) {
    widget_->setSelectedOutputDevice(deviceName);
}

void ConfigureDevicesPanel::setInputGainPercent(int percent) { widget_->setInputGainPercent(percent); }

void ConfigureDevicesPanel::setOutputGainPercent(int percent) { widget_->setOutputGainPercent(percent); }

void ConfigureDevicesPanel::setInputLevel(float level) { widget_->setInputLevel(level); }

void ConfigureDevicesPanel::setTestingInput(bool testing) { widget_->setTestingInput(testing); }

void ConfigureDevicesPanel::setTestingOutput(bool testing) { widget_->setTestingOutput(testing); }

void ConfigureDevicesPanel::setInputDeviceSelectionEnabled(bool enabled) {
    widget_->setInputDeviceSelectionEnabled(enabled);
}

void ConfigureDevicesPanel::setOutputDeviceSelectionEnabled(bool enabled) {
    widget_->setOutputDeviceSelectionEnabled(enabled);
}

}  // namespace sound_mind::studio
