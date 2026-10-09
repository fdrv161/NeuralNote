//
// Created by Damien Ronssin on 12.08.26.
//

#ifndef InstrumentMenu_h
#define InstrumentMenu_h

#include <optional>
#include <vector>

#include <JuceHeader.h>

#include "muscriptor/note.hpp"

class NeuralNoteAudioProcessor;

/**
 * The picker behind the sidebar's "+": every instrument the model can name, ticked where it is
 * selected, with "Automatic" at the top for the default of letting the model choose.
 *
 * A component rather than a juce::PopupMenu: the selection is a multi-select, so the panel has to
 * stay open across clicks.
 *
 * It covers the whole editor: everything outside the panel is the scrim that closes it.
 */
class InstrumentMenu : public juce::Component
{
public:
    explicit InstrumentMenu(NeuralNoteAudioProcessor& inProcessor);

    /** Puts the panel's top-right corner here, in this component's coordinates. */
    void setPanelAnchor(juce::Point<int> inTopRight);

    /** Fired when the menu closes itself: a click on the scrim, or Escape. */
    std::function<void()> onDismiss;

    void resized() override;

    void paint(juce::Graphics& g) override;

    void mouseDown(const juce::MouseEvent& inEvent) override;

    bool keyPressed(const juce::KeyPress& inKey) override;

    void visibilityChanged() override;

    /** A named group, so a screen reader says which panel it is in. */
    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override;

private:
    /** One offer in the list. No group means "Automatic", which is the empty selection. */
    struct Entry {
        juce::String name;
        std::optional<msl::InstrumentGroup> group;
    };

    class RowItem;

    /** The rows themselves, sized to their content so the viewport can scroll them. */
    class RowList : public juce::Component
    {
    public:
        RowList(NeuralNoteAudioProcessor& inProcessor, const std::vector<Entry>& inEntries);

        ~RowList() override;

        /** Creates one accessible item per entry. Called once the entries are filled in. */
        void createItems();

        int getIdealHeight() const;

        /** Ticks or unticks a row, as a click on it does. */
        void toggleRow(int inRow);

        /** Brings the accessible items' checked states in line with the stored selection. */
        void syncTicks();

        /** Moves the keyboard focus to a row, clamped to the list, and scrolls it into view. */
        void focusRow(int inRow);

        /** Focuses the row last focused, or the first ticked one when the menu opens. */
        void focusCurrentRow(bool inIsOpening);

        void resized() override;

        void paint(juce::Graphics& g) override;

        void mouseMove(const juce::MouseEvent& inEvent) override;

        void mouseExit(const juce::MouseEvent& inEvent) override;

        void mouseDown(const juce::MouseEvent& inEvent) override;

        std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override;

    private:
        int _rowAt(juce::Point<int> inPosition) const;

        static bool _isTicked(const Entry& inEntry, const std::vector<msl::InstrumentGroup>& inSelected);

        void _scrollRowIntoView(int inRow);

        NeuralNoteAudioProcessor& mProcessor;
        const std::vector<Entry>& mEntries;

        // Laid over the painted rows. Only the focused one wants keyboard focus, so Tab enters
        // the list once and the arrow keys move inside it.
        std::vector<std::unique_ptr<RowItem>> mItems;

        int mHoveredRow = -1;
        int mFocusedRow = 0;
    };

    /**
     * An invisible check box over one painted row, so a screen reader can read and tick it. The row
     * is still drawn and clicked through RowList; this only carries keyboard focus, keys and the
     * accessible name and state.
     */
    class RowItem : public juce::ToggleButton
    {
    public:
        RowItem(RowList& inOwner, int inRow, const juce::String& inName);

        void paintButton(juce::Graphics& g, bool inIsHighlighted, bool inIsDown) override;

        bool keyPressed(const juce::KeyPress& inKey) override;

        void focusGained(FocusChangeType inCause) override;

    private:
        RowList& mOwner;
        const int mRow;
    };

    juce::Rectangle<int> _panelBounds() const;

    std::vector<Entry> mEntries;

    juce::Viewport mViewport;
    RowList mRowList;

    juce::Point<int> mAnchor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InstrumentMenu)
};

#endif // InstrumentMenu_h
