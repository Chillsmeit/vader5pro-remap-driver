#pragma once

#include <iostream>

// NOLINTBEGIN(cppcoreguidelines-macro-usage,bugprone-macro-parentheses)
#ifndef NDEBUG
#define DBG(msg) (std::cerr << "[DEBUG] " << msg << "\n")
#else
#define DBG(msg) ((void)0)
#endif
// NOLINTEND(cppcoreguidelines-macro-usage,bugprone-macro-parentheses)