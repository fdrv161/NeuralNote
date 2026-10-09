//
// Accessibility helpers that fill gaps in JUCE's screen reader support.
//

#ifndef NnAccessibility_h
#define NnAccessibility_h

#include <functional>

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

/**
 * Has the screen reader speak a short message wherever the focus is, for news the user did not
 * move the focus to hear: transcription progress, a finished job.
 *
 * Raised as a UI Automation notification from inSource, which must be showing. JUCE's own
 * AccessibilityHandler::postAnnouncement speaks through a separate SAPI voice on Windows rather
 * than through the screen reader, so it is not used. A newer message replaces an unspoken older one.
 */
void announce(juce::Component& inSource, const juce::String& inMessage);

/**
 * A handler for a painted read-out (status line, time display): static text whose name is the text
 * it currently shows, built each time the screen reader asks. Return it from the component's
 * createAccessibilityHandler(), and give the component keyboard focus so it is a Tab stop.
 */
std::unique_ptr<juce::AccessibilityHandler> makeReadoutHandler(juce::Component& inComponent,
                                                               std::function<juce::String()> inText);
} // namespace nn::a11y

#endif // NnAccessibility_h
