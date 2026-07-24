#pragma once

// Protocol: the types and (de)serialization shared between server and client.
//
// The server owns the authoritative game state and serializes it to JSON; the
// client deserializes it into a local copy used purely for rendering. Messages
// on the wire are newline-delimited JSON objects.

#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "console_pet/mood.hpp"
#include "console_pet/pet.hpp"

namespace pet {

/// One line of the chat transcript.
struct ChatLine {
    std::string who;
    std::string text;
    bool is_pet = false;
};

/// Everything the client needs to render a frame.
struct RemoteState {
    Pet pet;                      // reconstructed pet (stats, deco, speech, events)
    Mood mood = Mood::Happy;      // server-computed mood
    std::vector<ChatLine> chat;   // chat transcript
    bool chat_pending = false;    // a chat reply is being generated
    std::string shop_status;      // last shop feedback message
};

/// Serialize the full server state to JSON.
nlohmann::json serialize_state(const Pet& pet, Mood mood,
                               const std::vector<ChatLine>& chat,
                               bool chat_pending,
                               const std::string& shop_status);

/// Parse a state JSON object into a RemoteState (client side).
RemoteState parse_state(const nlohmann::json& j);

}  // namespace pet
