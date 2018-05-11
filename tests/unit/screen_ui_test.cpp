/*
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stddef.h>

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <android-base/logging.h>
#include <gtest/gtest.h>

#include "common/test_constants.h"
#include "device.h"
#include "otautil/paths.h"
#include "private/resources.h"
#include "screen_ui.h"

static const std::vector<std::string> HEADERS{ "header" };
static const std::vector<std::string> ITEMS{ "item1", "item2", "item3", "item4", "1234567890" };

TEST(ScreenUITest, StartPhoneMenuSmoke) {
  Menu menu(false, 10, 20, HEADERS, ITEMS, 0);
  ASSERT_FALSE(menu.scrollable());
  ASSERT_EQ(HEADERS[0], menu.text_headers()[0]);
  ASSERT_EQ(5u, menu.ItemsCount());

  std::string message;
  ASSERT_FALSE(menu.ItemsOverflow(&message));
  for (size_t i = 0; i < menu.ItemsCount(); i++) {
    ASSERT_EQ(ITEMS[i], menu.TextItem(i));
  }

  ASSERT_EQ(0, menu.selection());
}

TEST(ScreenUITest, StartWearMenuSmoke) {
  Menu menu(true, 10, 8, HEADERS, ITEMS, 1);
  ASSERT_TRUE(menu.scrollable());
  ASSERT_EQ(HEADERS[0], menu.text_headers()[0]);
  ASSERT_EQ(5u, menu.ItemsCount());

  std::string message;
  ASSERT_FALSE(menu.ItemsOverflow(&message));
  for (size_t i = 0; i < menu.ItemsCount() - 1; i++) {
    ASSERT_EQ(ITEMS[i], menu.TextItem(i));
  }
  // Test of the last item is truncated
  ASSERT_EQ("12345678", menu.TextItem(4));
  ASSERT_EQ(1, menu.selection());
}

TEST(ScreenUITest, StartPhoneMenuItemsOverflow) {
  Menu menu(false, 1, 20, HEADERS, ITEMS, 0);
  ASSERT_FALSE(menu.scrollable());
  ASSERT_EQ(1u, menu.ItemsCount());

  std::string message;
  ASSERT_FALSE(menu.ItemsOverflow(&message));
  for (size_t i = 0; i < menu.ItemsCount(); i++) {
    ASSERT_EQ(ITEMS[i], menu.TextItem(i));
  }

  ASSERT_EQ(0u, menu.MenuStart());
  ASSERT_EQ(1u, menu.MenuEnd());
}

TEST(ScreenUITest, StartWearMenuItemsOverflow) {
  Menu menu(true, 1, 20, HEADERS, ITEMS, 0);
  ASSERT_TRUE(menu.scrollable());
  ASSERT_EQ(5u, menu.ItemsCount());

  std::string message;
  ASSERT_TRUE(menu.ItemsOverflow(&message));
  ASSERT_EQ("Current item: 1/5", message);

  for (size_t i = 0; i < menu.ItemsCount(); i++) {
    ASSERT_EQ(ITEMS[i], menu.TextItem(i));
  }

  ASSERT_EQ(0u, menu.MenuStart());
  ASSERT_EQ(1u, menu.MenuEnd());
}

TEST(ScreenUITest, PhoneMenuSelectSmoke) {
  int sel = 0;
  Menu menu(false, 10, 20, HEADERS, ITEMS, sel);
  // Mimic down button 10 times (2 * items size)
  for (int i = 0; i < 10; i++) {
    sel = menu.Select(++sel);
    ASSERT_EQ(sel, menu.selection());

    // Wraps the selection for unscrollable menu when it reaches the boundary.
    int expected = (i + 1) % 5;
    ASSERT_EQ(expected, menu.selection());

    ASSERT_EQ(0u, menu.MenuStart());
    ASSERT_EQ(5u, menu.MenuEnd());
  }

  // Mimic up button 10 times
  for (int i = 0; i < 10; i++) {
    sel = menu.Select(--sel);
    ASSERT_EQ(sel, menu.selection());

    int expected = (9 - i) % 5;
    ASSERT_EQ(expected, menu.selection());

    ASSERT_EQ(0u, menu.MenuStart());
    ASSERT_EQ(5u, menu.MenuEnd());
  }
}

TEST(ScreenUITest, WearMenuSelectSmoke) {
  int sel = 0;
  Menu menu(true, 10, 20, HEADERS, ITEMS, sel);
  // Mimic pressing down button 10 times (2 * items size)
  for (int i = 0; i < 10; i++) {
    sel = menu.Select(++sel);
    ASSERT_EQ(sel, menu.selection());

    // Stops the selection at the boundary if the menu is scrollable.
    int expected = std::min(i + 1, 4);
    ASSERT_EQ(expected, menu.selection());

    ASSERT_EQ(0u, menu.MenuStart());
    ASSERT_EQ(5u, menu.MenuEnd());
  }

  // Mimic pressing up button 10 times
  for (int i = 0; i < 10; i++) {
    sel = menu.Select(--sel);
    ASSERT_EQ(sel, menu.selection());

    int expected = std::max(3 - i, 0);
    ASSERT_EQ(expected, menu.selection());

    ASSERT_EQ(0u, menu.MenuStart());
    ASSERT_EQ(5u, menu.MenuEnd());
  }
}

TEST(ScreenUITest, WearMenuSelectItemsOverflow) {
  int sel = 1;
  Menu menu(true, 3, 20, HEADERS, ITEMS, sel);
  ASSERT_EQ(5u, menu.ItemsCount());

  // Scroll the menu to the end, and check the start & end of menu.
  for (int i = 0; i < 3; i++) {
    sel = menu.Select(++sel);
    ASSERT_EQ(i + 2, sel);
    ASSERT_EQ(static_cast<size_t>(i), menu.MenuStart());
    ASSERT_EQ(static_cast<size_t>(i + 3), menu.MenuEnd());
  }

  // Press down button one more time won't change the MenuStart() and MenuEnd().
  sel = menu.Select(++sel);
  ASSERT_EQ(4, sel);
  ASSERT_EQ(2u, menu.MenuStart());
  ASSERT_EQ(5u, menu.MenuEnd());

  // Scroll the menu to the top.
  // The expected menu sel, start & ends are:
  // sel 3, start 2, end 5
  // sel 2, start 2, end 5
  // sel 1, start 1, end 4
  // sel 0, start 0, end 3
  for (int i = 0; i < 4; i++) {
    sel = menu.Select(--sel);
    ASSERT_EQ(3 - i, sel);
    ASSERT_EQ(static_cast<size_t>(std::min(3 - i, 2)), menu.MenuStart());
    ASSERT_EQ(static_cast<size_t>(std::min(6 - i, 5)), menu.MenuEnd());
  }

  // Press up button one more time won't change the MenuStart() and MenuEnd().
  sel = menu.Select(--sel);
  ASSERT_EQ(0, sel);
  ASSERT_EQ(0u, menu.MenuStart());
  ASSERT_EQ(3u, menu.MenuEnd());
}

static constexpr int kMagicAction = 101;

enum class KeyCode : int {
  TIMEOUT = -1,
  NO_OP = 0,
  UP = 1,
  DOWN = 2,
  ENTER = 3,
  MAGIC = 1001,
  LAST,
};

static const std::map<KeyCode, int> kKeyMapping{
  // clang-format off
  { KeyCode::NO_OP, Device::kNoAction },
  { KeyCode::UP, Device::kHighlightUp },
  { KeyCode::DOWN, Device::kHighlightDown },
  { KeyCode::ENTER, Device::kInvokeItem },
  { KeyCode::MAGIC, kMagicAction },
  // clang-format on
};

class TestableScreenRecoveryUI : public ScreenRecoveryUI {
 public:
  int WaitKey() override;

  void SetKeyBuffer(const std::vector<KeyCode>& buffer);

  int KeyHandler(int key, bool visible) const;

  bool GetRtlLocale() const {
    return rtl_locale_;
  }

 private:
  std::vector<KeyCode> key_buffer_;
  size_t key_buffer_index_;
};

void TestableScreenRecoveryUI::SetKeyBuffer(const std::vector<KeyCode>& buffer) {
  key_buffer_ = buffer;
  key_buffer_index_ = 0;
}

int TestableScreenRecoveryUI::KeyHandler(int key, bool) const {
  KeyCode key_code = static_cast<KeyCode>(key);
  if (kKeyMapping.find(key_code) != kKeyMapping.end()) {
    return kKeyMapping.at(key_code);
  }
  return Device::kNoAction;
}

int TestableScreenRecoveryUI::WaitKey() {
  CHECK_LT(key_buffer_index_, key_buffer_.size());
  return static_cast<int>(key_buffer_[key_buffer_index_++]);
}

class ScreenRecoveryUITest : public ::testing::Test {
 protected:
  const std::string kTestLocale = "en-US";
  const std::string kTestRtlLocale = "ar";
  const std::string kTestRtlLocaleWithSuffix = "ar_EG";

  void SetUp() override {
    ui_ = std::make_unique<TestableScreenRecoveryUI>();

    std::string testdata_dir = from_testdata_base("");
    Paths::Get().set_resource_dir(testdata_dir);
    res_set_resource_dir(testdata_dir);

    ASSERT_TRUE(ui_->Init(kTestLocale));
  }

  std::unique_ptr<TestableScreenRecoveryUI> ui_;
};

TEST_F(ScreenRecoveryUITest, Init) {
  ASSERT_EQ(kTestLocale, ui_->GetLocale());
  ASSERT_FALSE(ui_->GetRtlLocale());
  ASSERT_FALSE(ui_->IsTextVisible());
  ASSERT_FALSE(ui_->WasTextEverVisible());
}

TEST_F(ScreenRecoveryUITest, ShowText) {
  ASSERT_FALSE(ui_->IsTextVisible());
  ui_->ShowText(true);
  ASSERT_TRUE(ui_->IsTextVisible());
  ASSERT_TRUE(ui_->WasTextEverVisible());

  ui_->ShowText(false);
  ASSERT_FALSE(ui_->IsTextVisible());
  ASSERT_TRUE(ui_->WasTextEverVisible());
}

TEST_F(ScreenRecoveryUITest, RtlLocale) {
  ASSERT_TRUE(ui_->Init(kTestRtlLocale));
  ASSERT_TRUE(ui_->GetRtlLocale());

  ASSERT_TRUE(ui_->Init(kTestRtlLocaleWithSuffix));
  ASSERT_TRUE(ui_->GetRtlLocale());
}

TEST_F(ScreenRecoveryUITest, ShowMenu) {
  ui_->SetKeyBuffer({
      KeyCode::UP,
      KeyCode::DOWN,
      KeyCode::UP,
      KeyCode::DOWN,
      KeyCode::ENTER,
  });
  ASSERT_EQ(3u, ui_->ShowMenu(HEADERS, ITEMS, 3, true,
                              std::bind(&TestableScreenRecoveryUI::KeyHandler, ui_.get(),
                                        std::placeholders::_1, std::placeholders::_2)));

  ui_->SetKeyBuffer({
      KeyCode::UP,
      KeyCode::UP,
      KeyCode::NO_OP,
      KeyCode::NO_OP,
      KeyCode::UP,
      KeyCode::ENTER,
  });
  ASSERT_EQ(2u, ui_->ShowMenu(HEADERS, ITEMS, 0, true,
                              std::bind(&TestableScreenRecoveryUI::KeyHandler, ui_.get(),
                                        std::placeholders::_1, std::placeholders::_2)));
}

TEST_F(ScreenRecoveryUITest, ShowMenu_NotMenuOnly) {
  ui_->SetKeyBuffer({
      KeyCode::MAGIC,
  });
  ASSERT_EQ(static_cast<size_t>(kMagicAction),
            ui_->ShowMenu(HEADERS, ITEMS, 3, false,
                          std::bind(&TestableScreenRecoveryUI::KeyHandler, ui_.get(),
                                    std::placeholders::_1, std::placeholders::_2)));
}

TEST_F(ScreenRecoveryUITest, ShowMenu_TimedOut) {
  ui_->SetKeyBuffer({
      KeyCode::TIMEOUT,
  });
  ASSERT_EQ(static_cast<size_t>(-1), ui_->ShowMenu(HEADERS, ITEMS, 3, true, nullptr));
}

TEST_F(ScreenRecoveryUITest, ShowMenu_TimedOut_TextWasEverVisible) {
  ui_->ShowText(true);
  ui_->ShowText(false);
  ASSERT_TRUE(ui_->WasTextEverVisible());

  ui_->SetKeyBuffer({
      KeyCode::TIMEOUT,
      KeyCode::DOWN,
      KeyCode::ENTER,
  });
  ASSERT_EQ(4u, ui_->ShowMenu(HEADERS, ITEMS, 3, true,
                              std::bind(&TestableScreenRecoveryUI::KeyHandler, ui_.get(),
                                        std::placeholders::_1, std::placeholders::_2)));
}
