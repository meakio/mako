#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <getopt.h>
#include <libgen.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <wordexp.h>
#include <unistd.h>

#include "config.h"
#include "criteria.h"
#include "types.h"
#include "wlr-layer-shell-unstable-v1-client-protocol.h"

void init_default_config(struct mako_config *config) {
	wl_list_init(&config->criteria);
	struct mako_criteria *global_criteria = create_criteria(config);
	init_default_style(&global_criteria->style);

	init_empty_style(&config->hidden_style);
	config->hidden_style.format = strdup("(%h more)");
	config->hidden_style.spec.format = true;

	config->output = strdup("");
	config->max_visible = 5;

	config->sort_criteria = MAKO_SORT_CRITERIA_TIME;
	config->sort_asc = 0;

	config->button_bindings.left = MAKO_BUTTON_BINDING_INVOKE_DEFAULT_ACTION;
	config->button_bindings.right = MAKO_BUTTON_BINDING_DISMISS;
	config->button_bindings.middle = MAKO_BUTTON_BINDING_NONE;

	config->anchor =
		ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP | ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT;
}

void finish_config(struct mako_config *config) {
	struct mako_criteria *criteria, *tmp;
	wl_list_for_each_safe(criteria, tmp, &config->criteria, link) {
		destroy_criteria(criteria);
	}

	finish_style(&config->hidden_style);
	free(config->output);
}

void init_default_style(struct mako_style *style) {
	style->width = 300;
	style->height = 100;

	style->margin.top = 10;
	style->margin.right = 10;
	style->margin.bottom = 10;
	style->margin.left = 10;

	style->padding = 5;
	style->border_size = 2;

	style->font = strdup("monospace 10");
	style->markup = true;
	style->format = strdup("<b>%s</b>\n%b");

	style->actions = true;
	style->default_timeout = 0;
	style->ignore_timeout = false;

	style->colors.background = 0x285577FF;
	style->colors.text = 0xFFFFFFFF;
	style->colors.border = 0x4C7899FF;

	// Everything in the default config is explicitly specified.
	memset(&style->spec, true, sizeof(struct mako_style_spec));
}

void init_empty_style(struct mako_style *style) {
	memset(style, 0, sizeof(struct mako_style));
}

void finish_style(struct mako_style *style) {
	free(style->font);
	free(style->format);
}

// Update `target` with the values specified in `style`. If a failure occurs,
// `target` will remain unchanged.
bool apply_style(struct mako_style *target, const struct mako_style *style) {
	// Try to duplicate strings up front in case allocation fails and we have
	// to bail without changing `target`.
	char *new_font = NULL;
	char *new_format = NULL;

	if (style->spec.font) {
		new_font = strdup(style->font);
		if (new_font == NULL) {
			fprintf(stderr, "allocation failed\n");
			return false;
		}
	}

	if (style->spec.format) {
		new_format = strdup(style->format);
		if (new_format == NULL) {
			fprintf(stderr, "allocation failed\n");
			return false;
		}
	}

	// Now on to actually setting things!

	if (style->spec.width) {
		target->width = style->width;
		target->spec.width = true;
	}

	if (style->spec.height) {
		target->height = style->height;
		target->spec.height = true;
	}

	if (style->spec.margin) {
		target->margin = style->margin;
		target->spec.margin = true;
	}

	if (style->spec.padding) {
		target->padding = style->padding;
		target->spec.padding = true;
	}

	if (style->spec.border_size) {
		target->border_size = style->border_size;
		target->spec.border_size = true;
	}

	if (style->spec.font) {
		free(target->font);
		target->font = new_font;
		target->spec.font = true;
	}

	if (style->spec.markup) {
		target->markup = style->markup;
		target->spec.markup = true;
	}

	if (style->spec.format) {
		free(target->format);
		target->format = new_format;
		target->spec.format = true;
	}

	if (style->spec.actions) {
		target->actions = style->actions;
		target->spec.actions = true;
	}

	if (style->spec.default_timeout) {
		target->default_timeout = style->default_timeout;
		target->spec.default_timeout = true;
	}

	if (style->spec.ignore_timeout) {
		target->ignore_timeout = style->ignore_timeout;
		target->spec.ignore_timeout = true;
	}

	if (style->spec.colors.background) {
		target->colors.background = style->colors.background;
		target->spec.colors.background = true;
	}

	if (style->spec.colors.text) {
		target->colors.text = style->colors.text;
		target->spec.colors.text = true;
	}

	if (style->spec.colors.border) {
		target->colors.border = style->colors.border;
		target->spec.colors.border = true;
	}

	return true;
}

static bool apply_config_option(struct mako_config *config, const char *name,
		const char *value) {
	if (strcmp(name, "max-visible") == 0) {
		return parse_int(value, &config->max_visible);
	} else if (strcmp(name, "output") == 0) {
		free(config->output);
		config->output = strdup(value);
		return true;
	} else if (strcmp(name, "sort") == 0) {
		if (strcmp(value, "+priority") == 0) {
			config->sort_criteria |= MAKO_SORT_CRITERIA_URGENCY;
			config->sort_asc |= MAKO_SORT_CRITERIA_URGENCY;
		} else if (strcmp(value, "-priority") == 0) {
			config->sort_criteria |= MAKO_SORT_CRITERIA_URGENCY;
			config->sort_asc &= ~MAKO_SORT_CRITERIA_URGENCY;
		} else if (strcmp(value, "+time") == 0) {
			config->sort_criteria |= MAKO_SORT_CRITERIA_TIME;
			config->sort_asc |= MAKO_SORT_CRITERIA_TIME;
		} else if (strcmp(value, "-time") == 0) {
			config->sort_criteria |= MAKO_SORT_CRITERIA_TIME;
			config->sort_asc &= ~MAKO_SORT_CRITERIA_TIME;
		}
		return true;
	} else if (strcmp(name, "anchor") == 0) {
		if (strcmp(value, "top-right") == 0) {
			config->anchor = ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP |
				ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT;
		} else if (strcmp(value, "bottom-right") == 0) {
			config->anchor = ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM |
				ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT;
		} else if (strcmp(value, "bottom-left") == 0) {
			config->anchor = ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM |
				ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT;
		} else if (strcmp(value, "top-left") == 0) {
			config->anchor = ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP |
				ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT;
		} else {
			return false;
		}
		return true;
	}

	return false;
}

static bool apply_style_option(struct mako_style *style, const char *name,
		const char *value) {
	struct mako_style_spec *spec = &style->spec;

	if (strcmp(name, "font") == 0) {
		free(style->font);
		return spec->font = !!(style->font = strdup(value));
	} else if (strcmp(name, "background-color") == 0) {
		return spec->colors.background =
			parse_color(value, &style->colors.background);
	} else if (strcmp(name, "text-color") == 0) {
		return spec->colors.text = parse_color(value, &style->colors.text);
	} else if (strcmp(name, "width") == 0) {
		return spec->width = parse_int(value, &style->width);
	} else if (strcmp(name, "height") == 0) {
		return spec->height = parse_int(value, &style->height);
	} else if (strcmp(name, "margin") == 0) {
		return spec->margin = parse_directional(value, &style->margin);
	} else if (strcmp(name, "padding") == 0) {
		return spec->padding = parse_int(value, &style->padding);
	} else if (strcmp(name, "border-size") == 0) {
		return spec->border_size = parse_int(value, &style->border_size);
	} else if (strcmp(name, "border-color") == 0) {
		return spec->colors.border = parse_color(value, &style->colors.border);
	} else if (strcmp(name, "markup") == 0) {
		return spec->markup = parse_boolean(value, &style->markup);
	} else if (strcmp(name, "format") == 0) {
		free(style->format);
		return spec->format = parse_format(value, &style->format);
	} else if (strcmp(name, "default-timeout") == 0) {
		return spec->default_timeout =
			parse_int(value, &style->default_timeout);
	} else if (strcmp(name, "ignore-timeout") == 0) {
		return spec->ignore_timeout =
			parse_boolean(value, &style->ignore_timeout);
	}

	return false;
}

static bool file_exists(const char *path) {
	return path && access(path, R_OK) != -1;
}

static char *get_config_path(void) {
	static const char *config_paths[] = {
		"$HOME/.mako/config",
		"$XDG_CONFIG_HOME/mako/config",
	};

	if (!getenv("XDG_CONFIG_HOME")) {
		char *home = getenv("HOME");
		if (!home) {
			return NULL;
		}
		char config_home[strlen(home) + strlen("/.config") + 1];
		strcpy(config_home, home);
		strcat(config_home, "/.config");
		setenv("XDG_CONFIG_HOME", config_home, 1);
	}

	for (size_t i = 0; i < sizeof(config_paths) / sizeof(char *); ++i) {
		wordexp_t p;
		if (wordexp(config_paths[i], &p, 0) == 0) {
			char *path = strdup(p.we_wordv[0]);
			wordfree(&p);
			if (file_exists(path)) {
				return path;
			}
			free(path);
		}
	}

	return NULL;
}

int load_config_file(struct mako_config *config) {
	char *path = get_config_path();
	if (!path) {
		return 0;
	}

	FILE *f = fopen(path, "r");
	if (!f) {
		fprintf(stderr, "Unable to open %s for reading", path);
		free(path);
		return -1;
	}
	const char *base = basename(path);

	int ret = 0;
	int lineno = 0;
	char *line = NULL;
	char *section = NULL;

	struct mako_criteria *criteria = global_criteria(config);

	size_t n = 0;
	while (getline(&line, &n, f) > 0) {
		++lineno;
		if (line[0] == '\0' || line[0] == '\n' || line[0] == '#') {
			continue;
		}

		if (line[strlen(line) - 1] == '\n') {
			line[strlen(line) - 1] = '\0';
		}

		if (line[0] == '[' && line[strlen(line) - 1] == ']') {
			free(section);
			section = strndup(line + 1, strlen(line) - 2);
			if (strcmp(section, "hidden") == 0) {
				// Skip making a criteria for the hidden section.
				criteria = NULL;
				continue;
			}
			criteria = create_criteria(config);
			if (!parse_criteria(section, criteria)) {
				fprintf(stderr, "[%s:%d] Invalid criteria definition\n", base,
						lineno);
				ret = -1;
				break;
			}
			continue;
		}

		char *eq = strchr(line, '=');
		if (!eq) {
			fprintf(stderr, "[%s:%d] Expected key=value\n", base, lineno);
			ret = -1;
			break;
		}

		bool valid_option = false;
		eq[0] = '\0';

		struct mako_style *target_style;
		if (section != NULL && strcmp(section, "hidden") == 0) {
			// The hidden criteria is a lie, we store the associated style
			// directly on the config because there's no "real" notification
			// object to match against it later.
			target_style = &config->hidden_style;
		} else {
			target_style = &criteria->style;
		}

		valid_option = apply_style_option(target_style, line, eq + 1);

		if (!valid_option && section == NULL) {
			valid_option = apply_config_option(config, line, eq + 1);
		}

		if (!valid_option) {
			fprintf(stderr, "[%s:%d] Failed to parse option '%s'\n",
				base, lineno, line);
			ret = -1;
			break;
		}
	}

	free(section);
	free(line);
	fclose(f);
	free(path);
	return ret;
}

static int config_argc = 0;
static char **config_argv = NULL;

int parse_config_arguments(struct mako_config *config, int argc, char **argv) {
	static const struct option long_options[] = {
		{"help", no_argument, 0, 'h'},
		{"font", required_argument, 0, 0},
		{"background-color", required_argument, 0, 0},
		{"text-color", required_argument, 0, 0},
		{"width", required_argument, 0, 0},
		{"height", required_argument, 0, 0},
		{"margin", required_argument, 0, 0},
		{"padding", required_argument, 0, 0},
		{"border-size", required_argument, 0, 0},
		{"border-color", required_argument, 0, 0},
		{"markup", required_argument, 0, 0},
		{"format", required_argument, 0, 0},
		{"max-visible", required_argument, 0, 0},
		{"default-timeout", required_argument, 0, 0},
		{"ignore-timeout", required_argument, 0, 0},
		{"output", required_argument, 0, 0},
		{"anchor", required_argument, 0, 0},
		{"sort", required_argument, 0, 0},
		{0},
	};

	optind = 1;
	while (1) {
		int option_index = -1;
		int c = getopt_long(argc, argv, "h", long_options, &option_index);
		if (c < 0) {
			break;
		} else if (c == 'h') {
			return 1;
		} else if (c != 0) {
			return -1;
		}

		const char *name = long_options[option_index].name;
		if (!apply_style_option(&global_criteria(config)->style, name, optarg)
				&& !apply_config_option(config, name, optarg)) {
			fprintf(stderr, "Failed to parse option '%s'\n", name);
			return -1;
		}
	}

	config_argc = argc;
	config_argv = argv;

	return 0;
}

bool reload_config(struct mako_config *config) {
	struct mako_config new_config = {0};
	init_default_config(&new_config);

	if (load_config_file(&new_config) != 0 ||
			parse_config_arguments(&new_config, config_argc, config_argv) != 0) {
		fprintf(stderr, "Failed to reload config\n");
		finish_config(&new_config);
		return false;
	}

	finish_config(config);
	*config = new_config;

	// We have to rebuild the wl_list that contains the criteria, as it is
	// currently pointing to local memory instead of the location of the real
	// criteria struct.
	wl_list_init(&config->criteria);
	wl_list_insert_list(&config->criteria, &new_config.criteria);

	return true;
}
