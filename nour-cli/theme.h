#pragma once
#include "types.h"

typedef struct {
	const char *reset;
	const char *bold;
	const char *dim;
	const char *italic;
	const char *underline;

	// Standard Terminal Colors (respects user terminal theme)
	const char *cyan;
	const char *green;
	const char *yellow;
	const char *red;
	const char *magenta;
	const char *gray;
	const char *white;

	// Color Aliases
	const char *teal;
	const char *emerald;
	const char *amber;
	const char *crimson;
	const char *lavender;
	const char *slate;
	const char *charcoal;

	// Box Drawing
	const char *top_left;
	const char *top_right;
	const char *bottom_left;
	const char *bottom_right;
	const char *horizontal;
	const char *vertical;
	const char *branch;
	const char *corner;
} Theme;

extern Theme th;

void theme_init(void);
bool theme_has_color(void);
