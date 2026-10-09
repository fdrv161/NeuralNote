//
// Created by Damien Ronssin on 12.08.26.
//

#include "InstrumentMenu.h"

#include <limits>

#include "InstrumentInfo.h"
#include "InstrumentSelection.h"
#include "NnFonts.h"
#include "NnLook.h"
#include "PluginProcessor.h"

namespace
{
const juce::String TITLE = "ADD INSTRUMENT";
const juce::String FOOTER = "TICK TO INCLUDE IN TRANSCRIPTION";

constexpr float TITLE_TRACKING = 0.13f;
constexpr float FOOTER_TRACKING = 0.04f;

constexpr int SHADOW_RADIUS = 34;
constexpr int SHADOW_DROP = 14;
} // namespace

InstrumentMenu::InstrumentMenu(NeuralNoteAudioProcessor& inProcessor)
    : mRowList(inProcessor, mEntries)
{
    // Nothing is selected by default and that means the model chooses, which is worth naming rather
    // than leaving as the state you get by unticking everything.
    mEntries.push_back({"Automatic (any instrument)", std::nullopt});

    for (const msl::InstrumentGroup group: msl::allInstrumentGroups()) {
        mEntries.push_back({instrumentDisplayForGroup(group).name, group});
    }

    mViewport.setViewedComponent(&mRowList, false);
    mViewport.setScrollBarsShown(true, false);

    auto& scrollbar = mViewport.getVerticalScrollBar();
    scrollbar.setColour(juce::ScrollBar::backgroundColourId, juce::Colours::transparentBlack);
    scrollbar.setColour(juce::ScrollBar::thumbColourId, nn::colours::scrollbarThumb);
    scrollbar.setColour(juce::ScrollBar::trackColourId, juce::Colours::transparentBlack);

    addAndMakeVisible(mViewport);

    // The rows take the arrow keys themselves, so the viewport is not a Tab stop of its own.
    mViewport.setWantsKeyboardFocus(false);

    mRowList.createItems();

    setWantsKeyboardFocus(true);

    // Tab stays inside the open menu rather than walking out to the controls under the scrim.
    setFocusContainerType(FocusContainerType::keyboardFocusContainer);

    setTitle("Add instrument");
    setDescription("Tick to include in transcription");
}

std::unique_ptr<juce::AccessibilityHandler> InstrumentMenu::createAccessibilityHandler()
{
    return std::make_unique<juce::AccessibilityHandler>(*this, juce::AccessibilityRole::group);
}

void InstrumentMenu::setPanelAnchor(juce::Point<int> inTopRight)
{
    mAnchor = inTopRight;
    resized();
}

juce::Rectangle<int> InstrumentMenu::_panelBounds() const
{
    const int list_height = juce::jmin(nn::metrics::menuListMaxHeight, mRowList.getIdealHeight());
    const int height = nn::metrics::menuHeaderHeight + list_height + nn::metrics::menuFooterHeight;

    return {mAnchor.x - nn::metrics::menuWidth, mAnchor.y, nn::metrics::menuWidth, height};
}

void InstrumentMenu::resized()
{
    auto panel = _panelBounds();
    panel.removeFromTop(nn::metrics::menuHeaderHeight);
    panel.removeFromBottom(nn::metrics::menuFooterHeight);

    mViewport.setBounds(panel);
    mRowList.setSize(mViewport.getMaximumVisibleWidth(), mRowList.getIdealHeight());
}

void InstrumentMenu::paint(juce::Graphics& g)
{
    auto panel = _panelBounds();

    juce::DropShadow(nn::colours::popupShadow(), SHADOW_RADIUS, {0, SHADOW_DROP}).drawForRectangle(g, panel);

    g.setColour(nn::colours::popupBg);
    g.fillRoundedRectangle(panel.toFloat(), nn::metrics::menuCorner);

    const auto header = panel.withHeight(nn::metrics::menuHeaderHeight);
    nn::drawBottomBorder(g, header, nn::colours::divStrong);

    g.setColour(nn::colours::popupTitle);
    nn::drawTrackedText(g,
                        TITLE,
                        nn::fonts::sectionHeader(),
                        header.reduced(nn::metrics::menuPadX, 0).toFloat(),
                        juce::Justification::centredLeft,
                        TITLE_TRACKING);

    const auto footer = panel.removeFromBottom(nn::metrics::menuFooterHeight);

    // Rounded along the bottom only, so its fill follows the panel's corners instead of squaring
    // them off.
    juce::Path footer_shape;
    footer_shape.addRoundedRectangle(footer.toFloat().getX(),
                                     footer.toFloat().getY(),
                                     footer.toFloat().getWidth(),
                                     footer.toFloat().getHeight(),
                                     nn::metrics::menuCorner,
                                     nn::metrics::menuCorner,
                                     false,
                                     false,
                                     true,
                                     true);

    g.setColour(nn::colours::popupFooterBg);
    g.fillPath(footer_shape);

    nn::drawTopBorder(g, footer, nn::colours::divStrong);

    g.setColour(nn::colours::textFainter);
    nn::drawTrackedText(g,
                        FOOTER,
                        nn::fonts::meta(),
                        footer.reduced(nn::metrics::menuPadX, 0).toFloat(),
                        juce::Justification::centredLeft,
                        FOOTER_TRACKING);

    g.setColour(nn::colours::popupBorder);
    g.drawRoundedRectangle(_panelBounds().toFloat().reduced(0.5f), nn::metrics::menuCorner, 1.0f);
}

void InstrumentMenu::mouseDown(const juce::MouseEvent& inEvent)
{
    // Everything outside the panel is the scrim. Clicks on the header and the footer land here too,
    // and are meant to do nothing.
    if (!_panelBounds().contains(inEvent.getPosition()) && onDismiss != nullptr) {
        onDismiss();
    }
}

bool InstrumentMenu::keyPressed(const juce::KeyPress& inKey)
{
    if (inKey == juce::KeyPress::escapeKey && onDismiss != nullptr) {
        onDismiss();
        return true;
    }

    // The panel itself has the focus after a click on its header or the scrim; the arrows take
    // it back into the list.
    if (inKey == juce::KeyPress::upKey || inKey == juce::KeyPress::downKey) {
        mRowList.focusCurrentRow(false);
        return true;
    }

    return false;
}

void InstrumentMenu::visibilityChanged()
{
    if (isVisible()) {
        // Taken so Escape reaches this rather than the main view's transport shortcuts. The main
        // view takes it back when the menu closes. Given to a row, which is inside this, so a
        // screen reader lands on something it can read and tick.
        mRowList.syncTicks();
        mRowList.focusCurrentRow(true);

        if (!hasKeyboardFocus(true)) {
            grabKeyboardFocus();
        }
    }
}

InstrumentMenu::RowList::RowList(NeuralNoteAudioProcessor& inProcessor, const std::vector<Entry>& inEntries)
    : mProcessor(inProcessor)
    , mEntries(inEntries)
{
    setTitle("Instruments");

    // A click ticks the row under it without moving the focus, which stays in the menu either way.
    setMouseClickGrabsKeyboardFocus(false);
}

InstrumentMenu::RowList::~RowList() = default;

void InstrumentMenu::RowList::createItems()
{
    for (int i = 0; i < static_cast<int>(mEntries.size()); i++) {
        auto item = std::make_unique<RowItem>(*this, i, mEntries[static_cast<std::size_t>(i)].name);
        item->setWantsKeyboardFocus(i == mFocusedRow);
        addAndMakeVisible(*item);
        mItems.push_back(std::move(item));
    }

    syncTicks();
}

std::unique_ptr<juce::AccessibilityHandler> InstrumentMenu::RowList::createAccessibilityHandler()
{
    return std::make_unique<juce::AccessibilityHandler>(*this, juce::AccessibilityRole::list);
}

void InstrumentMenu::RowList::resized()
{
    for (int i = 0; i < static_cast<int>(mItems.size()); i++) {
        mItems[static_cast<std::size_t>(i)]->setBounds(
            0, nn::metrics::menuListPadY + i * nn::metrics::menuRowHeight, getWidth(), nn::metrics::menuRowHeight);
    }
}

bool InstrumentMenu::RowList::_isTicked(const Entry& inEntry, const std::vector<msl::InstrumentGroup>& inSelected)
{
    return inEntry.group.has_value() ? std::find(inSelected.begin(), inSelected.end(), *inEntry.group)
                                           != inSelected.end()
                                     : inSelected.empty();
}

void InstrumentMenu::RowList::syncTicks()
{
    const std::vector<msl::InstrumentGroup> selected = InstrumentSelection::get(mProcessor.getValueTree());

    for (std::size_t i = 0; i < mItems.size(); i++) {
        mItems[i]->setToggleState(_isTicked(mEntries[i], selected), juce::dontSendNotification);
    }
}

void InstrumentMenu::RowList::toggleRow(int inRow)
{
    if (!juce::isPositiveAndBelow(inRow, static_cast<int>(mEntries.size()))) {
        return;
    }

    juce::ValueTree& state = mProcessor.getValueTree();
    const std::optional<msl::InstrumentGroup> group = mEntries[static_cast<std::size_t>(inRow)].group;

    if (group.has_value()) {
        InstrumentSelection::toggle(state, *group);
    } else {
        InstrumentSelection::clear(state);
    }

    syncTicks();

    // The menu stays open, so the row that was just ticked has to redraw itself here.
    repaint();
}

void InstrumentMenu::RowList::focusRow(int inRow)
{
    if (mItems.empty()) {
        return;
    }

    const int row = juce::jlimit(0, static_cast<int>(mItems.size()) - 1, inRow);

    mItems[static_cast<std::size_t>(mFocusedRow)]->setWantsKeyboardFocus(false);
    mFocusedRow = row;

    RowItem& item = *mItems[static_cast<std::size_t>(row)];
    item.setWantsKeyboardFocus(true);

    _scrollRowIntoView(row);

    if (!item.hasKeyboardFocus(false)) {
        item.grabKeyboardFocus();
    }

    // The hover highlight doubles as the keyboard cursor, so the focused row can be seen too.
    if (mHoveredRow != row) {
        mHoveredRow = row;
        repaint();
    }
}

void InstrumentMenu::RowList::focusCurrentRow(bool inIsOpening)
{
    int row = mFocusedRow;

    if (inIsOpening) {
        const std::vector<msl::InstrumentGroup> selected = InstrumentSelection::get(mProcessor.getValueTree());
        row = 0;

        for (int i = 0; i < static_cast<int>(mEntries.size()); i++) {
            if (_isTicked(mEntries[static_cast<std::size_t>(i)], selected)) {
                row = i;
                break;
            }
        }
    }

    focusRow(row);
}

void InstrumentMenu::RowList::_scrollRowIntoView(int inRow)
{
    auto* viewport = findParentComponentOfClass<juce::Viewport>();

    if (viewport == nullptr) {
        return;
    }

    const auto row_bounds = mItems[static_cast<std::size_t>(inRow)]->getBounds();
    const int view_top = viewport->getViewPositionY();
    const int view_height = viewport->getViewHeight();

    if (row_bounds.getY() < view_top) {
        viewport->setViewPosition(0, row_bounds.getY());
    } else if (row_bounds.getBottom() > view_top + view_height) {
        viewport->setViewPosition(0, row_bounds.getBottom() - view_height);
    }
}

InstrumentMenu::RowItem::RowItem(RowList& inOwner, int inRow, const juce::String& inName)
    : mOwner(inOwner)
    , mRow(inRow)
{
    setTitle(inName);

    // Its state follows the stored selection, which toggleRow writes and syncTicks reads back:
    // a click does not flip it on its own.
    setToggleable(true);
    setClickingTogglesState(false);
    onClick = [this] { mOwner.toggleRow(mRow); };

    // The rows under it are clicked through the list, as before.
    setInterceptsMouseClicks(false, false);
    setMouseClickGrabsKeyboardFocus(false);
}

void InstrumentMenu::RowItem::paintButton(juce::Graphics& g, bool inIsHighlighted, bool inIsDown)
{
    // The list paints the row.
    juce::ignoreUnused(g, inIsHighlighted, inIsDown);
}

bool InstrumentMenu::RowItem::keyPressed(const juce::KeyPress& inKey)
{
    if (inKey == juce::KeyPress::upKey) {
        mOwner.focusRow(mRow - 1);
        return true;
    }

    if (inKey == juce::KeyPress::downKey) {
        mOwner.focusRow(mRow + 1);
        return true;
    }

    if (inKey == juce::KeyPress::homeKey || inKey == juce::KeyPress::pageUpKey) {
        mOwner.focusRow(0);
        return true;
    }

    if (inKey == juce::KeyPress::endKey || inKey == juce::KeyPress::pageDownKey) {
        mOwner.focusRow(std::numeric_limits<int>::max());
        return true;
    }

    // Space ticks, as in any check box list, rather than reaching the main view's play / pause.
    if (inKey == juce::KeyPress::spaceKey) {
        triggerClick();
        return true;
    }

    // Return ticks too; Escape goes on up to the menu, which closes.
    return juce::ToggleButton::keyPressed(inKey);
}

void InstrumentMenu::RowItem::focusGained(FocusChangeType inCause)
{
    juce::ignoreUnused(inCause);

    // Focus can also arrive from a screen reader moving it directly.
    mOwner.focusRow(mRow);
}

int InstrumentMenu::RowList::getIdealHeight() const
{
    return static_cast<int>(mEntries.size()) * nn::metrics::menuRowHeight + 2 * nn::metrics::menuListPadY;
}

int InstrumentMenu::RowList::_rowAt(juce::Point<int> inPosition) const
{
    // Guarded rather than left to the division: negative offsets truncate toward zero, which would
    // put the top padding on the first row.
    if (inPosition.y < nn::metrics::menuListPadY) {
        return -1;
    }

    const int row = (inPosition.y - nn::metrics::menuListPadY) / nn::metrics::menuRowHeight;

    return juce::isPositiveAndBelow(row, static_cast<int>(mEntries.size())) ? row : -1;
}

void InstrumentMenu::RowList::paint(juce::Graphics& g)
{
    const std::vector<msl::InstrumentGroup> selected = InstrumentSelection::get(mProcessor.getValueTree());

    int y = nn::metrics::menuListPadY;

    for (int i = 0; i < static_cast<int>(mEntries.size()); i++) {
        const Entry& entry = mEntries[static_cast<std::size_t>(i)];

        const bool ticked = _isTicked(entry, selected);

        auto row = juce::Rectangle<int>(0, y, getWidth(), nn::metrics::menuRowHeight);
        y += nn::metrics::menuRowHeight;

        if (i == mHoveredRow) {
            g.setColour(nn::colours::popupRowHover);
            g.fillRect(row);
        }

        row = row.reduced(nn::metrics::menuPadX, 0);

        nn::drawCheckbox(g, row.removeFromRight(nn::metrics::checkboxSize), ticked);

        g.setColour(ticked ? nn::colours::popupItemTicked : nn::colours::popupItem);
        g.setFont(ticked ? nn::fonts::menuItemTicked() : nn::fonts::menuItem());
        g.drawText(entry.name, row.withTrimmedRight(nn::metrics::menuPadX), juce::Justification::centredLeft, true);
    }
}

void InstrumentMenu::RowList::mouseMove(const juce::MouseEvent& inEvent)
{
    const int row = _rowAt(inEvent.getPosition());

    if (row != mHoveredRow) {
        mHoveredRow = row;
        repaint();
    }
}

void InstrumentMenu::RowList::mouseExit(const juce::MouseEvent& inEvent)
{
    juce::ignoreUnused(inEvent);

    if (mHoveredRow != -1) {
        mHoveredRow = -1;
        repaint();
    }
}

void InstrumentMenu::RowList::mouseDown(const juce::MouseEvent& inEvent)
{
    toggleRow(_rowAt(inEvent.getPosition()));
}
