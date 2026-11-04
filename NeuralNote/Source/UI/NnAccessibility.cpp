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

class ToggleHandler final : public juce::AccessibilityHandler
{
public:
    ToggleHandler(juce::Button& inButton, juce::AccessibilityRole inRole)
        : juce::AccessibilityHandler(inButton, inRole, getActions(inButton))
        , mButton(inButton)
    {
    }

    juce::AccessibleState getCurrentState() const override
    {
        const auto state = juce::AccessibilityHandler::getCurrentState().withCheckable();
        return mButton.getToggleState() ? state.withChecked() : state;
    }

    juce::String getTitle() const override
    {
        const juce::String title = juce::AccessibilityHandler::getTitle();
        return title.isNotEmpty() ? title : mButton.getButtonText();
    }

    juce::String getHelp() const override { return mButton.getTooltip(); }

private:
    static juce::AccessibilityActions getActions(juce::Button& inButton)
    {
        return juce::AccessibilityActions()
            .addAction(juce::AccessibilityActionType::press, [&inButton] { inButton.triggerClick(); })
            .addAction(juce::AccessibilityActionType::toggle, [&inButton] {
                if (!inButton.isEnabled()) {
                    return;
                }

                // A list row keeps its tick in the data behind it, so only its click can change it.
                if (inButton.getClickingTogglesState()) {
                    inButton.setToggleState(!inButton.getToggleState(), juce::sendNotification);
                } else {
                    inButton.triggerClick();
                }
            });
    }

    juce::Button& mButton;
};
} // namespace

std::unique_ptr<juce::AccessibilityHandler> nn::a11y::makeReadoutHandler(juce::Component& inComponent,
                                                                         std::function<juce::String()> inText)
{
    return std::make_unique<ReadoutHandler>(inComponent, std::move(inText));
}

std::unique_ptr<juce::AccessibilityHandler> nn::a11y::makeToggleHandler(juce::Button& inButton,
                                                                        juce::AccessibilityRole inRole)
{
    return std::make_unique<ToggleHandler>(inButton, inRole);
}

void nn::a11y::moveFocusOn(juce::Component& inComponent)
{
    auto traverser = inComponent.createKeyboardFocusTraverser();

    if (traverser == nullptr) {
        return;
    }

    const auto is_usable = [&inComponent](const juce::Component* inTarget) {
        return inTarget != nullptr && inTarget != &inComponent && inTarget->isEnabled() && inTarget->isShowing();
    };

    juce::Component* target = traverser->getNextComponent(&inComponent);

    // Widening from the closest parent finds a working control near this one. The main view itself
    // takes the focus too, but as a bare group it tells the user nothing, so it is the last resort.
    for (auto* parent = inComponent.getParentComponent(); parent != nullptr && !is_usable(target);
         parent = parent->getParentComponent()) {
        target = traverser->getDefaultComponent(parent);
    }

    // A scroll area is a Tab stop too, but a bare group; the control after it is more use.
    const auto is_group = [](juce::Component* inTarget) {
        const auto* handler = inTarget->getAccessibilityHandler();
        return handler != nullptr && handler->getRole() == juce::AccessibilityRole::group;
    };

    juce::Component* after_groups = target;

    for (int i = 0; i < 8 && is_usable(after_groups) && is_group(after_groups); i++) {
        after_groups = traverser->getNextComponent(after_groups);
    }

    if (is_usable(after_groups)) {
        target = after_groups;
    }

    if (is_usable(target)) {
        target->grabKeyboardFocus();
    }
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

void nn::a11y::announceLater(juce::Component& inSource, const juce::String& inMessage)
{
    juce::Timer::callAfterDelay(500, [source = juce::Component::SafePointer<juce::Component>(&inSource), inMessage] {
        if (source != nullptr) {
            announce(*source, inMessage);
        }
    });
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
