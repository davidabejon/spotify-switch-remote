#pragma once
#include <pu/Plutonium>
#include <functional>
#include <string>

class LanguageSelectLayout : public pu::ui::Layout {
public:
    using OnLanguageSelectedCallback = std::function<void(const std::string&)>;

private:
    OnLanguageSelectedCallback onSelected;
    int selectedIndex = 0;

    pu::ui::elm::TextBlock::Ref titleText;
    pu::ui::elm::TextBlock::Ref subtitleText;
    pu::ui::elm::Rectangle::Ref optionBg[2];
    pu::ui::elm::TextBlock::Ref optionText[2];
    pu::ui::elm::Image::Ref     flagImg[2];
    pu::ui::elm::TextBlock::Ref hintText;
    bool flagsLoaded = false;

    void UpdateSelection();
    void OnInputCallback(const u64 keys_down);
    void LoadFlags();

public:
    explicit LanguageSelectLayout(OnLanguageSelectedCallback cb);
    PU_SMART_CTOR(LanguageSelectLayout)
};
