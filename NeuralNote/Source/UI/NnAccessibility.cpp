//
// Accessibility helpers that fill gaps in JUCE's screen reader support.
//

#include "NnAccessibility.h"

#if JUCE_WINDOWS
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <windows.h>
    #include <uiautomation.h>

namespace juce
{
// Defined by JUCE in juce_Accessibility_windows.cpp, which is compiled into this plugin. It is not
// in a public header, so it is declared here with the same signature.
void sendAccessibilityPropertyChangedEvent(const AccessibilityHandler&, PROPERTYID, VARIANT);
} // namespace juce
#endif

namespace
{
class ReadoutHandler final : public juce::AccessibilityHandler
{
public:
    ReadoutHandler(juce::Component& inComponent, std::function<juce::String()> inText)
        : juce::AccessibilityHandler(inComponent, juce::AccessibilityRole::staticText)
        , mText(std::move(inText))
    {
    }

    juce::String getTitle() const override { return mText(); }

private:
    std::function<juce::String()> mText;
};
} // namespace

std::unique_ptr<juce::AccessibilityHandler> nn::a11y::makeReadoutHandler(juce::Component& inComponent,
                                                                         std::function<juce::String()> inText)
{
    return std::make_unique<ReadoutHandler>(inComponent, std::move(inText));
}

void nn::a11y::notifyToggleStateChanged(juce::Component& inComponent)
{
#if JUCE_WINDOWS
    auto* handler = inComponent.getAccessibilityHandler();

    if (handler == nullptr) {
        return;
    }

    const juce::AccessibleState state = handler->getCurrentState();

    if (!state.isCheckable()) {
        return;
    }

    VARIANT new_value;
    VariantInit(&new_value);
    new_value.vt = VT_I4;
    new_value.lVal = state.isChecked() ? ToggleState_On : ToggleState_Off;

    juce::sendAccessibilityPropertyChangedEvent(*handler, UIA_ToggleToggleStatePropertyId, new_value);
#else
    juce::ignoreUnused(inComponent);
#endif
}
