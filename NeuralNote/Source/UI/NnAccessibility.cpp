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

namespace
{
// UiaRaiseNotificationEvent is looked up at run time: it only exists from Windows 10 1709 on, and
// the SDK declares it and its enums only for that target.
using RaiseNotificationEventFn = HRESULT(WINAPI*)(IRawElementProviderSimple*, int, int, BSTR, BSTR);

constexpr int NOTIFICATION_KIND_OTHER = 4;
constexpr int NOTIFICATION_PROCESSING_MOST_RECENT = 3;

RaiseNotificationEventFn getRaiseNotificationEvent()
{
    static const RaiseNotificationEventFn function = []() -> RaiseNotificationEventFn {
        HMODULE module = LoadLibraryW(L"UIAutomationCore.dll");

        if (module == nullptr) {
            return nullptr;
        }

        return reinterpret_cast<RaiseNotificationEventFn>(GetProcAddress(module, "UiaRaiseNotificationEvent"));
    }();

    return function;
}
} // namespace
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

void nn::a11y::announce(juce::Component& inSource, const juce::String& inMessage)
{
#if JUCE_WINDOWS
    const RaiseNotificationEventFn raise_notification_event = getRaiseNotificationEvent();
    auto* handler = inSource.getAccessibilityHandler();

    if (raise_notification_event == nullptr || handler == nullptr || inMessage.isEmpty()) {
        return;
    }

    auto* native = handler->getNativeImplementation();

    if (native == nullptr) {
        return;
    }

    // JUCE's native element inherits from IRawElementProviderSimple first, so the object starts with
    // that interface; QueryInterface then hands back the properly adjusted pointer.
    auto* unknown = reinterpret_cast<IUnknown*>(native);
    IRawElementProviderSimple* provider = nullptr;

    if (FAILED(unknown->QueryInterface(__uuidof(IRawElementProviderSimple), reinterpret_cast<void**>(&provider)))
        || provider == nullptr) {
        return;
    }

    BSTR message = SysAllocString(inMessage.toWideCharPointer());
    BSTR activity_id = SysAllocString(L"NeuralNote");

    raise_notification_event(
        provider, NOTIFICATION_KIND_OTHER, NOTIFICATION_PROCESSING_MOST_RECENT, message, activity_id);

    SysFreeString(message);
    SysFreeString(activity_id);
    provider->Release();
#else
    juce::ignoreUnused(inSource, inMessage);
#endif
}
