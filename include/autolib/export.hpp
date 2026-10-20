// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (c) 2025-2026 Julien Menana

#pragma once

// Symbols are exported from a Windows DLL; elsewhere the default visibility applies
#if defined(_WIN32) && defined(AUTOLIB_SHARED)
#if defined(AUTOLIB_BUILDING)
#define AUTOLIB_EXPORT __declspec(dllexport)
#else
#define AUTOLIB_EXPORT __declspec(dllimport)
#endif
#else
#define AUTOLIB_EXPORT
#endif
