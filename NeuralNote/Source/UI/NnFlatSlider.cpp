//
// Created by Damien Ronssin on 07.08.26.
//

#include "NnFlatSlider.h"

#include "NnLook.h"

int NnFlatSlider::ThumbLayout::getSliderThumbRadius(juce::Slider&)
{
    return THUMB_INDENT;
}

NnFlatSlider::NnFlatSlider()
    : juce::Slider(juce::Slider::LinearHorizontal, juce::Slider::NoTextBox)
{
    setLookAndFeel(&mThumbLayout.get());

    setColour(trackColourId, nn::colours::faderTrack);
    setColour(fillColourId, nn::colours::accent);
    setColour(thumbColourId, nn::colours::faderThumb);

    setMouseCursor(juce::MouseCursor::PointingHandCursor);

    // juce::Slider opts out of keyboard focus by default. Opted back in so Tab, the arrow keys and
    // screen readers can reach it; a click still leaves the focus where it was.
    setWantsKeyboardFocus(true);
    setMouseClickGrabsKeyboardFocus(false);
}

NnFlatSlider::~NnFlatSlider()
{
    // Before ~Slider, which would otherwise outlive the look and feel it still points at.
    setLookAndFeel(nullptr);
}

bool NnFlatSlider::keyPressed(const juce::KeyPress& inKey)
{
    // Ctrl+arrows belong to the main view, which moves the playhead.
    if (inKey.getModifiers().isAnyModifierKeyDown()) {
        return juce::Slider::keyPressed(inKey);
    }

    const double step = mKeyStep > 0.0 ? mKeyStep : (getMaximum() - getMinimum()) / 100.0;
    double new_value = getValue();

    if (inKey == juce::KeyPress::rightKey || inKey == juce::KeyPress::upKey) {
        new_value += step;
    } else if (inKey == juce::KeyPress::leftKey || inKey == juce::KeyPress::downKey) {
        new_value -= step;
    } else if (inKey == juce::KeyPress::pageUpKey) {
        new_value += 10.0 * step;
    } else if (inKey == juce::KeyPress::pageDownKey) {
        new_value -= 10.0 * step;
    } else if (inKey == juce::KeyPress::homeKey) {
        new_value = getMaximum();
    } else if (inKey == juce::KeyPress::endKey) {
        new_value = getMinimum();
    } else {
        return juce::Slider::keyPressed(inKey);
    }

    // Wraps the change in a gesture, so a host records it as automation like a drag.
    const ScopedDragNotification drag(*this);
    setValue(juce::jlimit(getMinimum(), getMaximum(), new_value), juce::sendNotificationSync);

    return true;
}

void NnFlatSlider::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const float alpha = isEnabled() ? 1.0f : nn::DISABLED_ALPHA;

    // The same travel juce::Slider maps the drag across, so the thumb sits under the cursor.
    const float indent = static_cast<float>(THUMB_INDENT);
    const float travel = juce::jmax(1.0f, bounds.getWidth() - 2.0f * indent);
    const float proportion = static_cast<float>(valueToProportionOfLength(getValue()));
    const float thumb_centre = bounds.getX() + indent + proportion * travel;

    const auto track = juce::Rectangle<float>(bounds.getX(), 0.0f, bounds.getWidth(), TRACK_HEIGHT)
                           .withCentre({bounds.getCentreX(), bounds.getCentreY()});

    g.setColour(findColour(trackColourId).withMultipliedAlpha(alpha));
    g.fillRoundedRectangle(track, TRACK_HEIGHT / 2.0f);

    const float fill_width = thumb_centre - track.getX();

    if (fill_width > 0.0f) {
        g.setColour(findColour(fillColourId).withMultipliedAlpha(alpha));
        g.fillRoundedRectangle(track.withWidth(fill_width), TRACK_HEIGHT / 2.0f);
    }

    g.setColour(findColour(thumbColourId).withMultipliedAlpha(alpha));
    g.fillRoundedRectangle(
        juce::Rectangle<float>(THUMB_WIDTH, THUMB_HEIGHT).withCentre({thumb_centre, bounds.getCentreY()}),
        THUMB_WIDTH / 2.0f);
}
