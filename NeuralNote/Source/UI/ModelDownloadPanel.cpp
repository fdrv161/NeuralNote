//
// Created by Damien Ronssin on 12.09.26.
//

#include "ModelDownloadPanel.h"

#include "NeuralNoteTooltips.h"
#include "NnAccessibility.h"
#include "NNFileUtils.h"
#include "NnFonts.h"
#include "NnGlobalSettings.h"
#include "NnIcons.h"
#include "NnLook.h"
#include "PluginProcessor.h"

namespace
{
constexpr int PANEL_WIDTH = 440;
constexpr float PANEL_CORNER = 8.0f;

// Header, rows' contents and footer all start here, so they share one left edge.
constexpr int CONTENT_X = 20;

constexpr int PAD_TOP = 16;
constexpr int PAD_BOTTOM = 14;
constexpr int TITLE_HEIGHT = 18;
constexpr int SUBTITLE_HEIGHT = 16;
constexpr int HEADER_GAP = 10;

// A row's background reaches closer to the panel's edge than its contents do.
constexpr int ROW_INSET_X = 10;
constexpr int ROW_HEIGHT = 46;
constexpr int ROW_GAP = 2;
constexpr float ROW_CORNER = 6.0f;
constexpr int ROW_NAME_TOP = 8;
constexpr int ROW_META_TOP = 26;
constexpr int ROW_LINE_HEIGHT = 14;
constexpr int CHECKBOX_GAP = 12;

// Wide enough for a progress bar, its percentage and the cross, and every other control is
// right-aligned in the same column.
constexpr int CONTROL_COLUMN_WIDTH = 172;
constexpr int CONTROL_GAP = 10;
constexpr int PERCENT_WIDTH = 32;

constexpr int FOOTER_GAP = 10;
constexpr int BUTTON_HEIGHT = 26;
constexpr float ICON_SIZE = 13.0f;
constexpr float BAR_CORNER = 2.0f;
constexpr int POLL_HZ = 10;

const juce::String VERIFYING_CAPTION = "VERIFYING";
constexpr float CAPTION_TRACKING = 0.06f;

/** Relative to the other sizes: they are the same model at three scales. */
juce::String hintFor(ModelSize inModelSize)
{
    switch (inModelSize) {
        case ModelSize::Small:
            return "Fastest";
        case ModelSize::Medium:
            return "Recommended";
        case ModelSize::Large:
            return "Largest, slowest";
    }

    return {};
}

int percentOf(const ModelDownloader::Status& inStatus)
{
    return juce::roundToInt(100.0f * inStatus.getProgress());
}

/** e.g. "618 MB", "2.7 GB". */
juce::String formatSize(juce::int64 inNumBytes)
{
    if (inNumBytes >= 1'000'000'000) {
        return juce::String(static_cast<double>(inNumBytes) / 1.0e9, 1) + " GB";
    }

    return juce::String(juce::roundToInt(static_cast<double>(inNumBytes) / 1.0e6)) + " MB";
}

/** A row's contents: its background minus the inset that lines them up with the header. */
juce::Rectangle<int> contentOf(juce::Rectangle<int> inRowBounds)
{
    return inRowBounds.reduced(CONTENT_X - ROW_INSET_X, 0);
}
} // namespace

ModelDownloadPanel::Row::Row(ModelSize inModelSize, ModelDownloadPanel& inOwner, int inRow)
    : modelSize(inModelSize)
    , item(inOwner, inRow)
    , downloadButton("DownloadModel")
    , cancelButton("StopModelDownload")
{
}

ModelDownloadPanel::ModelDownloadPanel(NeuralNoteAudioProcessor& inProcessor)
    : mProcessor(inProcessor)
{
    for (const ModelSize size: ALL_MODEL_SIZES) {
        auto row = std::make_unique<Row>(size, *this, static_cast<int>(size));

        // Added first, so the row's buttons stay in front of it.
        addAndMakeVisible(row->item);

        // The Transcribe button's colours: it is the same call to action, one step earlier.
        row->downloadButton.setIcon(nn::icons::downloadStroked, NnFlatButton::IconStyle::stroked, ICON_SIZE);
        row->downloadButton.setPadding(10, 12, 7);
        row->downloadButton.setCornerRadius(static_cast<float>(nn::metrics::controlCorner));
        row->downloadButton.setColour(NnFlatButton::backgroundColourId, nn::colours::ctaFill());
        row->downloadButton.setColour(NnFlatButton::outlineColourId, nn::colours::ctaBorder);
        row->downloadButton.setColour(NnFlatButton::iconColourId, nn::colours::ctaText);
        row->downloadButton.setColour(NnFlatButton::textColourId, nn::colours::ctaText);
        row->downloadButton.onClick = [this, size] {
            mProcessor.getModelDownloader().start(size);
            timerCallback();
        };
        addChildComponent(row->downloadButton);

        row->cancelButton.setIcon(
            nn::icons::crossStroked, NnFlatButton::IconStyle::stroked, nn::metrics::cancelGlyphSize);
        row->cancelButton.setCornerRadius(4.0f);
        row->cancelButton.setTooltip(NeuralNoteTooltips::stop_model_download);
        row->cancelButton.setTitle("Stop downloading " + juce::String(modelSizeToDisplayName(size)));
        row->cancelButton.onClick = [this, size] { mProcessor.getModelDownloader().cancel(size); };
        addChildComponent(row->cancelButton);

        mRows[static_cast<size_t>(size)] = std::move(row);
    }

    mCloseButton.setIcon(nn::icons::crossStroked, NnFlatButton::IconStyle::stroked, nn::metrics::cancelGlyphSize);
    mCloseButton.setCornerRadius(4.0f);
    mCloseButton.setTooltip("Close");
    mCloseButton.setTitle("Close model panel");
    mCloseButton.onClick = [this] {
        if (onCloseRequested != nullptr) {
            onCloseRequested();
        }
    };
    addAndMakeVisible(mCloseButton);

    mOpenFolderButton.setIcon(nn::icons::folderStroked, NnFlatButton::IconStyle::stroked, ICON_SIZE);
    mOpenFolderButton.setLabel("Open models folder", nn::fonts::buttonLabel());
    mOpenFolderButton.setTitle("Open models folder");
    mOpenFolderButton.setPadding(10, 12, 7);
    mOpenFolderButton.setCornerRadius(static_cast<float>(nn::metrics::controlCorner));
    mOpenFolderButton.setColour(NnFlatButton::backgroundColourId, nn::colours::popupRowHover);
    mOpenFolderButton.setColour(NnFlatButton::outlineColourId, nn::colours::popupBorder);
    mOpenFolderButton.setColour(NnFlatButton::iconColourId, nn::colours::textIcon);
    mOpenFolderButton.setColour(NnFlatButton::textColourId, nn::colours::textButton);
    mOpenFolderButton.onClick = [] { NNFileUtils::openModelsDirectory(); };
    addAndMakeVisible(mOpenFolderButton);

    setTitle("Transcription model");
    setDescription("Tick the model to transcribe with, or download another");

    mHasInstalledModel = NNFileUtils::isAnyModelInstalled();
    _updateRows(true);

    // Also while hidden: noticing the last checkpoint disappear is what brings the panel back.
    startTimerHz(POLL_HZ);
}

int ModelDownloadPanel::getIdealWidth()
{
    return PANEL_WIDTH;
}

int ModelDownloadPanel::getIdealHeight()
{
    const int num_rows = static_cast<int>(ALL_MODEL_SIZES.size());

    return PAD_TOP + TITLE_HEIGHT + SUBTITLE_HEIGHT + HEADER_GAP + num_rows * ROW_HEIGHT + (num_rows - 1) * ROW_GAP
           + FOOTER_GAP + BUTTON_HEIGHT + PAD_BOTTOM;
}

void ModelDownloadPanel::setCloseButtonVisible(bool inVisible)
{
    mCloseButton.setVisible(inVisible);
}

void ModelDownloadPanel::timerCallback()
{
    const bool has_installed_model = NNFileUtils::isAnyModelInstalled();
    const bool installed_changed = has_installed_model != mHasInstalledModel;
    mHasInstalledModel = has_installed_model;

    // The title follows whether anything is installed, so that alone is worth a repaint.
    if (_updateRows(false) || installed_changed) {
        repaint();
    }

    if (installed_changed && onInstalledModelsChanged != nullptr) {
        onInstalledModelsChanged();
    }
}

bool ModelDownloadPanel::_updateRows(bool inForce)
{
    bool changed = inForce;
    const std::optional<ModelSize> in_use = NNFileUtils::getInstalledModelSize(NnGlobalSettings::getModelSize());

    for (auto& row: mRows) {
        ModelDownloader::Status status = mProcessor.getModelDownloader().getStatus(row->modelSize);
        const bool is_installed = NNFileUtils::isModelInstalled(row->modelSize);
        const bool is_in_use = in_use == row->modelSize;
        const ModelDownloader::Status& shown = row->status;

        changed = changed || is_installed != row->isInstalled || is_in_use != row->isInUse
                  || status.phase != shown.phase || percentOf(status) != percentOf(shown)
                  || (status.downloadedBytes > 0) != (shown.downloadedBytes > 0)
                  || status.errorMessage != shown.errorMessage;

        _announceDownload(*row, status, is_installed);

        row->status = std::move(status);
        row->isInstalled = is_installed;
        row->isInUse = is_in_use;
    }

    if (!changed) {
        return false;
    }

    for (auto& row: mRows) {
        const ModelDownloader::Status& status = row->status;

        row->downloadButton.setLabel(status.phase == ModelDownloader::Phase::Failed ? "Retry"
                                     : status.downloadedBytes > 0                   ? "Resume"
                                                                                    : "Download",
                                     nn::fonts::buttonLabel());
        // The label alone does not say which model the button is for.
        row->downloadButton.setTitle((status.phase == ModelDownloader::Phase::Failed ? "Retry "
                                      : status.downloadedBytes > 0                   ? "Resume "
                                                                                     : "Download ")
                                     + juce::String(modelSizeToDisplayName(row->modelSize)));
        const bool show_download = !row->isInstalled && !status.isBusy();
        const bool show_cancel = !row->isInstalled && status.phase == ModelDownloader::Phase::Downloading;

        // A button that hides while it has the focus would drop it on the panel, so it goes to the
        // row instead, whose name says what happened.
        if ((!show_download && row->downloadButton.hasKeyboardFocus(false))
            || (!show_cancel && row->cancelButton.hasKeyboardFocus(false))) {
            row->item.grabKeyboardFocus();
        }

        row->downloadButton.setVisible(show_download);

        const juce::String description = _describeRow(*row);

        if (row->item.getTitle() != description) {
            row->item.setTitle(description);
        }

        if (row->item.getToggleState() != row->isInUse) {
            row->item.setToggleState(row->isInUse, juce::dontSendNotification);
            nn::a11y::notifyToggleStateChanged(row->item);
        }
        row->cancelButton.setVisible(show_cancel);
    }

    // A relabelled button is a resized one.
    resized();
    return true;
}

void ModelDownloadPanel::resized()
{
    auto area = getLocalBounds();
    area.removeFromTop(PAD_TOP);

    mCloseButton.setBounds(area.withHeight(TITLE_HEIGHT)
                               .withTrimmedRight(CONTENT_X)
                               .removeFromRight(nn::metrics::cancelHitSize)
                               .withSizeKeepingCentre(nn::metrics::cancelHitSize, nn::metrics::cancelHitSize));

    area.removeFromTop(TITLE_HEIGHT + SUBTITLE_HEIGHT + HEADER_GAP);

    for (auto& row: mRows) {
        row->bounds = area.removeFromTop(ROW_HEIGHT).reduced(ROW_INSET_X, 0);
        row->item.setBounds(row->bounds);
        area.removeFromTop(ROW_GAP);

        auto column = contentOf(row->bounds).removeFromRight(CONTROL_COLUMN_WIDTH);

        const int button_width = row->downloadButton.getIdealWidth();
        row->downloadButton.setBounds(
            column.withLeft(column.getRight() - button_width).withSizeKeepingCentre(button_width, BUTTON_HEIGHT));

        row->cancelButton.setBounds(column.withLeft(column.getRight() - nn::metrics::cancelHitSize)
                                        .withSizeKeepingCentre(nn::metrics::cancelHitSize, nn::metrics::cancelHitSize));
    }

    area.removeFromTop(FOOTER_GAP - ROW_GAP);

    const int folder_width = mOpenFolderButton.getIdealWidth();
    mOpenFolderButton.setBounds(area.removeFromTop(BUTTON_HEIGHT).withTrimmedLeft(CONTENT_X).withWidth(folder_width));
}

void ModelDownloadPanel::paint(juce::Graphics& g)
{
    const auto surface = getLocalBounds().toFloat().reduced(0.5f);
    g.setColour(nn::colours::popupBg);
    g.fillRoundedRectangle(surface, PANEL_CORNER);
    g.setColour(nn::colours::popupBorder);
    g.drawRoundedRectangle(surface, PANEL_CORNER, 1.0f);

    auto header = getLocalBounds().reduced(CONTENT_X, 0);
    header.removeFromTop(PAD_TOP);

    g.setColour(nn::colours::textPrimary);
    g.setFont(nn::fonts::filename());
    g.drawText(mHasInstalledModel ? "Transcription model" : "No transcription model installed",
               header.removeFromTop(TITLE_HEIGHT).withTrimmedRight(nn::metrics::cancelHitSize + CONTROL_GAP),
               juce::Justification::centredLeft,
               true);

    g.setColour(nn::colours::textLabel);
    g.setFont(nn::fonts::menuItem());
    g.drawText(mHasInstalledModel ? "Tick the model to transcribe with." : "Download a model to start transcribing.",
               header.removeFromTop(SUBTITLE_HEIGHT),
               juce::Justification::centredLeft,
               true);

    for (int i = 0; i < static_cast<int>(mRows.size()); i++) {
        const Row& row = *mRows[static_cast<size_t>(i)];
        const ModelDownloader::Status& status = row.status;

        if (row.isInUse) {
            g.setColour(nn::colours::accentFillActive());
            g.fillRoundedRectangle(row.bounds.toFloat(), ROW_CORNER);
        } else if (row.isInstalled && i == mHoveredRow) {
            g.setColour(nn::colours::popupRowHover);
            g.fillRoundedRectangle(row.bounds.toFloat(), ROW_CORNER);
        }

        auto content = contentOf(row.bounds);

        nn::drawCheckbox(g,
                         content.removeFromLeft(nn::metrics::checkboxSize),
                         row.isInUse,
                         row.isInstalled ? 1.0f : nn::DISABLED_ALPHA);
        content.removeFromLeft(CHECKBOX_GAP);

        // What the controls on the right leave for the name and the line under it.
        auto column = content;
        column.removeFromLeft(content.getWidth() - CONTROL_COLUMN_WIDTH);
        auto text = content;

        if (row.isInstalled) {
            // Nothing on the right: the whole row is the control.
        } else if (status.phase == ModelDownloader::Phase::Downloading) {
            text.setRight(column.getX() - CONTROL_GAP);

            column.removeFromRight(nn::metrics::cancelHitSize + CONTROL_GAP);

            g.setColour(nn::colours::progressText);
            g.setFont(nn::fonts::statusBar());
            g.drawText(juce::String(percentOf(status)) + "%",
                       column.removeFromRight(PERCENT_WIDTH),
                       juce::Justification::centredRight);

            column.removeFromRight(CONTROL_GAP);

            const auto bar = column.withSizeKeepingCentre(column.getWidth(), nn::metrics::progressBarHeight).toFloat();

            g.setColour(nn::colours::progressTrack);
            g.fillRoundedRectangle(bar, BAR_CORNER);

            const float filled = bar.getWidth() * status.getProgress();

            if (filled > 0.0f) {
                g.setColour(nn::colours::progressFill);
                g.fillRoundedRectangle(bar.withWidth(std::max(filled, 2.0f * BAR_CORNER)), BAR_CORNER);
            }
        } else if (status.phase == ModelDownloader::Phase::Verifying) {
            text.setRight(column.getX() - CONTROL_GAP);

            g.setColour(nn::colours::progressText);
            nn::drawTrackedText(g,
                                VERIFYING_CAPTION,
                                nn::fonts::statusBar(),
                                column.toFloat(),
                                juce::Justification::centredRight,
                                CAPTION_TRACKING);
        } else {
            // Up to the button rather than the column, so an error message gets what room there is.
            text.setRight(row.downloadButton.getX() - CONTROL_GAP);
        }

        g.setColour(row.isInUse       ? nn::colours::popupItemTicked
                    : row.isInstalled ? nn::colours::popupItem
                                      : nn::colours::textLabel);
        g.setFont(row.isInUse ? nn::fonts::menuItemTicked() : nn::fonts::instrumentName());
        g.drawText(modelSizeToDisplayName(row.modelSize),
                   text.withTrimmedTop(ROW_NAME_TOP).withHeight(ROW_LINE_HEIGHT),
                   juce::Justification::centredLeft,
                   true);

        const bool failed = !row.isInstalled && status.phase == ModelDownloader::Phase::Failed;
        const bool busy = !row.isInstalled && status.isBusy();
        const juce::String separator = "  " + nn::separatorDot() + "  ";

        const juce::String meta = failed ? status.errorMessage
                                  : busy ? formatSize(status.downloadedBytes) + " of " + formatSize(status.totalBytes)
                                         : formatSize(status.totalBytes) + separator + hintFor(row.modelSize);

        g.setColour(failed ? nn::colours::warn : nn::colours::textFaint);
        g.setFont(nn::fonts::meta());
        g.drawText(meta,
                   text.withTrimmedTop(ROW_META_TOP).withHeight(ROW_LINE_HEIGHT),
                   juce::Justification::centredLeft,
                   true);
    }
}

int ModelDownloadPanel::_rowAt(juce::Point<int> inPosition) const
{
    for (int i = 0; i < static_cast<int>(mRows.size()); i++) {
        if (mRows[static_cast<size_t>(i)]->bounds.contains(inPosition)) {
            return i;
        }
    }

    return -1;
}

void ModelDownloadPanel::mouseMove(const juce::MouseEvent& inEvent)
{
    const int hovered = _rowAt(inEvent.getPosition());
    const bool can_pick = hovered >= 0 && mRows[static_cast<size_t>(hovered)]->isInstalled;

    setMouseCursor(can_pick ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);

    if (hovered != mHoveredRow) {
        mHoveredRow = hovered;
        repaint();
    }
}

void ModelDownloadPanel::mouseExit(const juce::MouseEvent&)
{
    if (mHoveredRow != -1) {
        mHoveredRow = -1;
        repaint();
    }
}

void ModelDownloadPanel::mouseUp(const juce::MouseEvent& inEvent)
{
    const int clicked = _rowAt(inEvent.getPosition());

    if (!inEvent.mouseWasClicked() || clicked < 0) {
        return;
    }

    const Row& row = *mRows[static_cast<size_t>(clicked)];

    if (row.isInstalled && !row.isInUse) {
        NnGlobalSettings::setModelSize(row.modelSize);
        timerCallback();
    }
}

bool ModelDownloadPanel::keyPressed(const juce::KeyPress& inKey)
{
    // Escape closes the panel the way its cross does, when it has one: a panel shown because no
    // model is installed stays.
    if (inKey == juce::KeyPress::escapeKey && mCloseButton.isVisible()) {
        if (onCloseRequested != nullptr) {
            onCloseRequested();
        }

        return true;
    }

    return false;
}

void ModelDownloadPanel::visibilityChanged()
{
    if (!isVisible()) {
        return;
    }

    // The focus goes to the model in use, or to the first row when none is installed, so a screen
    // reader starts on the choice the panel is for rather than having to Tab its way in.
    int target = 0;

    for (int i = 0; i < static_cast<int>(mRows.size()); i++) {
        if (mRows[static_cast<size_t>(i)]->isInUse) {
            target = i;
            break;
        }
    }

    _focusRow(target);
}

std::unique_ptr<juce::AccessibilityHandler> ModelDownloadPanel::createAccessibilityHandler()
{
    return std::make_unique<juce::AccessibilityHandler>(*this, juce::AccessibilityRole::group);
}

void ModelDownloadPanel::_pickRow(int inRow)
{
    if (!juce::isPositiveAndBelow(inRow, static_cast<int>(mRows.size()))) {
        return;
    }

    Row& row = *mRows[static_cast<size_t>(inRow)];

    if (row.isInstalled) {
        if (!row.isInUse) {
            NnGlobalSettings::setModelSize(row.modelSize);
            timerCallback();
        }
    } else if (row.downloadButton.isVisible()) {
        row.downloadButton.triggerClick();
    }
}

void ModelDownloadPanel::_focusRow(int inRow)
{
    const int row = juce::jlimit(0, static_cast<int>(mRows.size()) - 1, inRow);

    mRows[static_cast<size_t>(row)]->item.grabKeyboardFocus();
}

juce::String ModelDownloadPanel::_describeRow(const Row& inRow) const
{
    const ModelDownloader::Status& status = inRow.status;
    const juce::String name = modelSizeToDisplayName(inRow.modelSize);

    if (inRow.isInstalled) {
        return name + ", " + hintFor(inRow.modelSize) + ", " + formatSize(status.totalBytes);
    }

    // No percentage: the name is re-read whenever it changes, and a running count would be spoken
    // every second while the row has the focus.
    switch (status.phase) {
        case ModelDownloader::Phase::Downloading:
            return name + ", downloading";
        case ModelDownloader::Phase::Verifying:
            return name + ", verifying the download";
        case ModelDownloader::Phase::Failed:
            return name + ", download failed: " + status.errorMessage;
        default:
            break;
    }

    return name + ", not installed, " + formatSize(status.totalBytes) + ", " + hintFor(inRow.modelSize);
}

ModelDownloadPanel::RowItem::RowItem(ModelDownloadPanel& inOwner, int inRow)
    : mOwner(inOwner)
    , mRow(inRow)
{
    // Its tick follows the model in use, which _updateRows reads back: a press does not flip it on
    // its own.
    setToggleable(true);
    setClickingTogglesState(false);
    onClick = [this] { mOwner._pickRow(mRow); };

    // The rows under it are clicked through the panel, as before.
    setInterceptsMouseClicks(false, false);
    setMouseClickGrabsKeyboardFocus(false);
    setWantsKeyboardFocus(true);
}

void ModelDownloadPanel::RowItem::paintButton(juce::Graphics& g, bool inIsHighlighted, bool inIsDown)
{
    // The panel paints the row.
    juce::ignoreUnused(g, inIsHighlighted, inIsDown);
}

bool ModelDownloadPanel::RowItem::keyPressed(const juce::KeyPress& inKey)
{
    if (inKey == juce::KeyPress::upKey) {
        mOwner._focusRow(mRow - 1);
        return true;
    }

    if (inKey == juce::KeyPress::downKey) {
        mOwner._focusRow(mRow + 1);
        return true;
    }

    if (inKey == juce::KeyPress::homeKey) {
        mOwner._focusRow(0);
        return true;
    }

    if (inKey == juce::KeyPress::endKey) {
        mOwner._focusRow(static_cast<int>(mOwner.mRows.size()) - 1);
        return true;
    }

    // Space picks, as in any check box list, rather than reaching the main view's play / pause.
    if (inKey == juce::KeyPress::spaceKey || inKey == juce::KeyPress::returnKey) {
        triggerClick();
        return true;
    }

    // Escape goes on up to the panel, which closes.
    return juce::ToggleButton::keyPressed(inKey);
}

void ModelDownloadPanel::RowItem::focusGained(FocusChangeType inCause)
{
    juce::ignoreUnused(inCause);

    // The hover highlight doubles as the keyboard cursor, so the focused row can be seen too.
    if (mOwner.mHoveredRow != mRow) {
        mOwner.mHoveredRow = mRow;
        mOwner.repaint();
    }
}

void ModelDownloadPanel::_announceDownload(Row& inRow,
                                           const ModelDownloader::Status& inStatus,
                                           bool inIsInstalled)
{
    const ModelDownloader::Status& shown = inRow.status;
    const juce::String name = modelSizeToDisplayName(inRow.modelSize);
    juce::String message;

    if (inStatus.phase == ModelDownloader::Phase::Downloading) {
        if (shown.phase != ModelDownloader::Phase::Downloading) {
            // A resumed download starts its count from where it already is, so it is not spoken.
            inRow.announcedQuarters = percentOf(inStatus) / 25;
        }

        const int quarters = percentOf(inStatus) / 25;

        if (quarters > inRow.announcedQuarters && quarters < 4) {
            inRow.announcedQuarters = quarters;
            message = name + ", " + juce::String(quarters * 25) + "%";
        }
    } else if (shown.isBusy() && !inStatus.isBusy()) {
        if (inIsInstalled) {
            message = name + " model downloaded";
        } else if (inStatus.phase == ModelDownloader::Phase::Failed) {
            message = name + " download failed: " + inStatus.errorMessage;
        } else {
            message = name + " download stopped";
        }
    }

    // Through the window rather than the panel: the panel may have been closed while it downloaded.
    if (message.isNotEmpty()) {
        if (auto* window = getTopLevelComponent()) {
            nn::a11y::announce(*window, message);
        }
    }
}
