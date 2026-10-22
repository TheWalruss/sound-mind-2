#include "test_video_export_dialog.h"

#include <filesystem>

#include <QComboBox>
#include <QLabel>
#include <QSettings>
#include <QtTest/QtTest>

#include "sound_mind/codec/video_options.h"
#include "sound_mind/studio/video_export_dialog.h"

using sound_mind::codec::VideoAnimation;
using sound_mind::codec::VideoCodec;
using sound_mind::studio::VideoExportDialog;

namespace {

std::filesystem::path suggested() { return std::filesystem::temp_directory_path() / "Piece_export.mp4"; }

void forgetChoices() {
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("SoundMind"),
                       QStringLiteral("SoundMindStudio"));
    settings.remove(QStringLiteral("videoExport"));
}

}  // namespace

// v0.1.45.1 - the export dialog, modelled on the legacy Studio's.
void VideoExportDialogTest::offersOnlyAvailableFormatsAndPrefersH264() {
    forgetChoices();
    VideoExportDialog dialog(QStringLiteral("Export Video"), suggested(), false, 1024, 512);
    auto* formats = dialog.findChild<QComboBox*>(QStringLiteral("videoExportFormat"));
    QVERIFY(formats != nullptr);
    QCOMPARE(formats->count(), static_cast<int>(sound_mind::codec::availableVideoCodecs().size()));
    QCOMPARE(dialog.options().codec, sound_mind::codec::preferredVideoCodec());
    QCOMPARE(dialog.options().frameRate, 30);
    QCOMPARE(dialog.options().quality, sound_mind::codec::VideoQuality::Medium);
}

void VideoExportDialogTest::theExtensionFollowsTheFormat() {
    forgetChoices();
    VideoExportDialog dialog(QStringLiteral("Export Video"), suggested(), false, 1024, 512);
    dialog.setCodec(VideoCodec::ProRes);
    QCOMPARE(dialog.outputPath().extension().string(), std::string(".mov"));
    QCOMPARE(dialog.outputPath().stem().string(), std::string("Piece_export"));
    dialog.setCodec(VideoCodec::Mpeg4);
    QCOMPARE(dialog.outputPath().extension().string(), std::string(".mp4"));
}

void VideoExportDialogTest::resolutionPresetsCanvasSizeAndCustomSize() {
    forgetChoices();
    VideoExportDialog dialog(QStringLiteral("Export Video"), suggested(), false, 1024, 512);
    QCOMPARE(dialog.options().width, 1920u);  // 1080p by default.
    QCOMPARE(dialog.options().height, 1080u);
    dialog.setResolutionPreset(4);  // vertical.
    QCOMPARE(dialog.options().width, 1080u);
    QCOMPARE(dialog.options().height, 1920u);
    dialog.setResolutionPreset(7);  // the canvas's own size.
    QCOMPARE(dialog.options().width, 0u);
    QCOMPARE(dialog.options().height, 0u);
    dialog.setResolutionPreset(8);  // custom.
    dialog.setCustomSize(800, 600);
    QCOMPARE(dialog.options().width, 800u);
    QCOMPARE(dialog.options().height, 600u);
}

void VideoExportDialogTest::scrollingAndThePolarBanner() {
    forgetChoices();
    VideoExportDialog flat(QStringLiteral("Export Video"), suggested(), false, 1024, 512);
    QCOMPARE(flat.options().animation, VideoAnimation::WholeCanvas);
    QVERIFY(flat.findChild<QLabel*>(QStringLiteral("polarBanner"))->isHidden());
    flat.setScrolling(12.5);
    QCOMPARE(flat.options().animation, VideoAnimation::Scrolling);
    QCOMPARE(flat.options().windowSeconds, 12.5);
    QVERIFY(!flat.options().polar);

    VideoExportDialog polar(QStringLiteral("Export Video"), suggested(), true, 1024, 512);
    QVERIFY(polar.options().polar);
    QVERIFY(!polar.findChild<QLabel*>(QStringLiteral("polarBanner"))->isHidden());
}

void VideoExportDialogTest::choicesAreRememberedForTheNextExport() {
    forgetChoices();
    {
        VideoExportDialog dialog(QStringLiteral("Export Video"), suggested(), false, 1024, 512);
        dialog.setCodec(VideoCodec::ProRes);
        dialog.setResolutionPreset(3);  // 720p.
        dialog.setScrolling(20.0);
        dialog.accept();
    }
    VideoExportDialog again(QStringLiteral("Export Video"), suggested(), false, 1024, 512);
    QCOMPARE(again.options().codec, VideoCodec::ProRes);
    QCOMPARE(again.options().width, 1280u);
    QCOMPARE(again.options().animation, VideoAnimation::Scrolling);
    QCOMPARE(again.options().windowSeconds, 20.0);
    QCOMPARE(again.outputPath().extension().string(), std::string(".mov"));
    forgetChoices();
}
