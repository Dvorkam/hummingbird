#pragma once

#include <SDL.h>

#include "core/platform_api/InputEvent.h"

namespace Hummingbird::Platform::SDLInput {

bool translate_event(const SDL_Event& e, InputEvent& out);

}  // namespace Hummingbird::Platform::SDLInput
