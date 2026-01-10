#pragma once

#include <SDL.h>

#include "core/platform_api/InputEvent.h"

bool translate_event(const SDL_Event& e, InputEvent& out);
