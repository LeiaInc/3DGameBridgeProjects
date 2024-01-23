#include "pch.h"

#include <cstdint>
#include <windows.h>

class HotKey {
public:
    HotKey();
    HotKey(bool isEnabled, shortcutType type, uint8_t key, bool shiftRequired, bool altRequired, bool ctrlRequired);
    void toggle_hot_key() { is_enabled = !is_enabled; }
    //Unused for now, can make the key editable with this later.
    void set_toggle_key(uint8_t newKeyValue, shortcutType type, bool shiftRequired, bool altRequired, bool ctrlRequired);

    bool get_enabled();
    bool get_shift_required();
    bool get_alt_required();
    bool get_ctrl_required();
    shortcutType get_type();
    uint8_t get_key();
    uint8_t get_id();

private:
    bool is_enabled = true;
    bool shift_required = false;
    bool alt_required = false;
    bool ctrl_required = true;
    shortcutType type = shortcutType::toggle_SR;
    uint8_t shortcut_key = 0x00;
};
