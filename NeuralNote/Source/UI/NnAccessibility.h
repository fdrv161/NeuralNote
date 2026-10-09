//
// Accessibility helpers that fill gaps in JUCE's screen reader support.
//

#ifndef NnAccessibility_h
#define NnAccessibility_h

#include <JuceHeader.h>

namespace nn::a11y
{
/**
 * Tells the screen reader that a check box was ticked or unticked.
 *
 * On Windows, JUCE reports the new tick state to UI Automation only when the screen reader itself
 * toggles the control. A tick made with Space or Return from the plugin's own key handling
 * changes the state silently, so NVDA says nothing until the focus leaves and comes back. Call
 * this after changing the toggle state of a toggleable button. It does nothing elsewhere.
 */
void notifyToggleStateChanged(juce::Component& inComponent);
} // namespace nn::a11y

#endif // NnAccessibility_h
