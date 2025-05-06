// fakeeeprom.h
#pragma once
#include <stdint.h>
#include <map>

extern std::map<uint16_t, uint16_t> fakeEEPROM;

inline uint16_t eeprom_read_word(uint16_t* addr) {
    return fakeEEPROM[*addr];
}

inline void eeprom_write_word(uint16_t* addr, uint16_t val) {
    fakeEEPROM[*addr] = val;
}
