//
// Accessibility support for NeuralNote.
//

#ifndef NnGroupViewport_h
#define NnGroupViewport_h

#include <JuceHeader.h>

/**
 * A juce::Viewport that screen readers announce as a named group.
 *
 * A plain viewport takes keyboard focus, for its arrow-key scrolling, but exposes no role, so NVDA
 * reads it as "unknown". Give it a title with setTitle.
 */
class NnGroupViewport : public juce::Viewport
{
public:
    using juce::Viewport::Viewport;

    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override
    {
        return std::make_unique<juce::AccessibilityHandler>(*this, juce::AccessibilityRole::group);
    }
};

#endif // NnGroupViewport_h
