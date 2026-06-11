#include "ServiceMenu.h"

namespace {

// Ikony 8x8 w formacie XBM (LSB = lewy piksel) — 1:1 z makiety oled-preview.
const uint8_t kIconTest[] = {0x7C, 0x70, 0x48, 0x44, 0x02, 0x01, 0x00, 0x00};
const uint8_t kIconInfo[] = {0x18, 0x18, 0x00, 0x1C, 0x18, 0x18, 0x18, 0x3C};
const uint8_t kIconClock[] = {0x3C, 0x42, 0x91, 0x91, 0xB1, 0x81, 0x42, 0x3C};
const uint8_t kIconUpdate[] = {0x3C, 0x42, 0x81, 0x80, 0x91, 0x61, 0xE2, 0x4C};
const uint8_t kIconGear[] = {0x24, 0x7E, 0xC3, 0x5A, 0x5A, 0xC3, 0x7E, 0x24};
const uint8_t kIconTara[] = {0x18, 0x24, 0x7E, 0x42, 0x42, 0x42, 0x42, 0x7E};
const uint8_t kIconExit[] = {0x3E, 0x22, 0x22, 0xA2, 0xE2, 0xA2, 0x22, 0x3E};

struct MenuItem {
  const uint8_t* icon;
  const char* label;
  ServiceMenuAction action;
};

const MenuItem kItems[] = {
    {kIconTest, "Test WS", ServiceMenuAction::TEST_WS},
    {kIconInfo, "Info", ServiceMenuAction::INFO},
    {kIconClock, "Czas", ServiceMenuAction::TIME},
    {kIconUpdate, "Aktualizacja", ServiceMenuAction::UPDATE},
    {kIconGear, "Kalibracja", ServiceMenuAction::CALIBRATION},
    {kIconTara, "Zapis tary", ServiceMenuAction::TARA_SAVE},
    {kIconExit, "Wyjscie", ServiceMenuAction::EXIT},
};

const uint8_t kItemCount = sizeof(kItems) / sizeof(kItems[0]);

}  // namespace

ServiceMenu::ServiceMenu(Display& display)
    : display_(display), cursor_(0), scrollTop_(0), dirty_(false) {}

void ServiceMenu::enter(const char* ipText) {
  ip_ = ipText != nullptr ? ipText : "";
  cursor_ = 0;
  scrollTop_ = 0;
  draw();
}

void ServiceMenu::requestRedraw() { dirty_ = true; }

void ServiceMenu::draw() {
  U8G2& g = display_.u8g2();
  g.clearBuffer();
  g.setFont(u8g2_font_6x10_tf);

  // Nagłówek w negatywie: SERWIS + adres IP portalu.
  g.setDrawColor(1);
  g.drawBox(0, 0, 128, 11);
  g.setDrawColor(0);
  g.setCursor(2, 9);
  g.print("SERWIS ");
  g.print(ip_);
  g.setDrawColor(1);

  for (uint8_t row = 0; row < kVisibleRows; ++row) {
    const uint8_t idx = scrollTop_ + row;
    if (idx >= kItemCount) {
      break;
    }
    const int y = 12 + row * 9;
    if (idx == cursor_) {
      g.drawBox(0, y, 128, 9);
      g.setDrawColor(0);
    }
    g.drawXBM(2, y + 1, 8, 8, kItems[idx].icon);
    g.setCursor(13, y + 8);
    g.print(kItems[idx].label);
    g.setDrawColor(1);
  }

  g.setCursor(2, 64);
  g.print("1/2=ruch  OK=wybierz");
  g.sendBuffer();
}

ServiceMenuAction ServiceMenu::tick(InputButtons& buttons) {
  if (buttons.wasPressed(BTN_1)) {
    cursor_ = (cursor_ == 0) ? (uint8_t)(kItemCount - 1) : (uint8_t)(cursor_ - 1);
    dirty_ = true;
  } else if (buttons.wasPressed(BTN_2)) {
    cursor_ = (uint8_t)((cursor_ + 1) % kItemCount);
    dirty_ = true;
  } else if (buttons.wasPressed(BTN_OK)) {
    return kItems[cursor_].action;
  } else if (buttons.wasPressed(BTN_TARA)) {
    return ServiceMenuAction::EXIT;
  }

  if (dirty_) {
    if (cursor_ < scrollTop_) {
      scrollTop_ = cursor_;
    }
    if (cursor_ >= scrollTop_ + kVisibleRows) {
      scrollTop_ = (uint8_t)(cursor_ - kVisibleRows + 1);
    }
    dirty_ = false;
    draw();
  }
  return ServiceMenuAction::NONE;
}
