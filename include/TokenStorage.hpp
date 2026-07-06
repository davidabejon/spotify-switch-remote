#pragma once
#include <SpotifyAuth.hpp>

namespace TokenStorage {

bool saveTokens(const spotify::Tokens& tokens);
spotify::Tokens loadTokens();
void clearTokens();

} // namespace TokenStorage
