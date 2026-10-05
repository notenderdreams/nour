#include "theme.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static const Theme terminal_theme = {
	.reset	   = "\033[0m",
	.bold	   = "\033[1m",
	.dim	   = "\033[2m",
	.italic	   = "\033[3m",
	.underline = "\033[4m",

	.cyan	 = "\033[36m",
	.green	 = "\033[32m",
	.yellow	 = "\033[33m",
	.red	 = "\033[31m",
	.magenta = "\033[35m",
	.gray	 = "\033[90m",
	.white	 = "\033[1m",

	.teal	  = "\033[36m",
	.emerald  = "\033[32m",
	.amber	  = "\033[33m",
	.crimson  = "\033[31m",
	.lavender = "\033[35m",
	.slate	  = "\033[90m",
	.charcoal = "",

	.top_left	  = "╭",
	.top_right	  = "╮",
	.bottom_left  = "╰",
	.bottom_right = "╯",
	.horizontal	  = "─",
	.vertical	  = "│",
	.branch		  = "├─",
	.corner		  = "└─",
};

static const Theme plain_theme = {
	.reset	   = "",
	.bold	   = "",
	.dim	   = "",
	.italic	   = "",
	.underline = "",

	.cyan	 = "",
	.green	 = "",
	.yellow	 = "",
	.red	 = "",
	.magenta = "",
	.gray	 = "",
	.white	 = "",

	.teal	  = "",
	.emerald  = "",
	.amber	  = "",
	.crimson  = "",
	.lavender = "",
	.slate	  = "",
	.charcoal = "",

	.top_left	  = "+",
	.top_right	  = "+",
	.bottom_left  = "+",
	.bottom_right = "+",
	.horizontal	  = "-",
	.vertical	  = "|",
	.branch		  = "|--",
	.corner		  = "`--",
};

Theme		th			= { 0 };
static bool s_has_color = false;

void theme_init(void)
{
	bool		is_tty	 = isatty(STDOUT_FILENO);
	bool		no_color = (getenv("NO_COLOR") != NULL);
	const char *term	 = getenv("TERM");
	bool		dumb	 = (term && strcmp(term, "dumb") == 0);

	if (is_tty && !no_color && !dumb) {
		th			= terminal_theme;
		s_has_color = true;
	} else {
		th			= plain_theme;
		s_has_color = false;
	}
}

bool theme_has_color(void)
{
	return s_has_color;
}
