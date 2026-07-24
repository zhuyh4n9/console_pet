#pragma once

// Save / load of the pet to a JSON file.
//
// The on-disk format matches the original Python version exactly, so existing
// pet_save.json files keep working.

#include <optional>
#include <string>

#include "console_pet/pet.hpp"

namespace pet {

/// Default save file (relative to the current working directory).
inline constexpr const char* kSaveFile = "pet_save.json";

void save_pet(const Pet& pet, const std::string& path = kSaveFile);

/// Load a pet, returning std::nullopt on any I/O or parse error.
std::optional<Pet> load_pet(const std::string& path = kSaveFile);

void delete_save(const std::string& path = kSaveFile);

}  // namespace pet
