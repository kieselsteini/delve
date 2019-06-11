/*
================================================================================

	delve - a simple terminal gopher client
    Copyright (C) 2019  Sebastian Steinhauer

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.

================================================================================
*/
/*============================================================================*/
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <netdb.h>
#include <unistd.h>


/*============================================================================*/
typedef struct Selector {
	struct Selector *next;
	int index;
	char type, *name, *host, *port, *path;
} Selector;

typedef struct Variable {
	struct Variable *next;
	char *name, *data;
} Variable;

typedef struct Command {
	const char *name;
	void (*func)(char *line);
} Command;

typedef struct Help {
	const char *name;
	const char *text;
} Help;


/*============================================================================*/
Variable *variables = NULL;
Selector *bookmarks = NULL;
Selector *history = NULL;
Selector *menu = NULL;


/*============================================================================*/
void vlogf(const char *color, const char *fmt, va_list va) {
	printf("\33[%sm", color);
	vprintf(fmt, va);
	puts("\33[0m");
}

void info(const char *fmt, ...) {
	va_list va;
	va_start(va, fmt);
	vlogf("34", fmt, va);
	va_end(va);
}

void error(const char *fmt, ...) {
	va_list va;
	va_start(va, fmt);
	vlogf("31", fmt, va);
	va_end(va);
}

void panic(const char *fmt, ...) {
	va_list va;
	va_start(va, fmt);
	vlogf("31", fmt, va);
	va_end(va);
	exit(EXIT_FAILURE);
}


/*============================================================================*/
void str_free(char *str) {
	if (str != NULL && *str != '\0') free(str);
}

char *str_copy(char *str) {
	char *new;
	if (str == NULL || *str == '\0') return "";
	if ((new = strdup(str)) == NULL) panic("cannot allocate new string");
	return new;
}

char *str_skip(char *str, const char *delim) {
	while (*str && strchr(delim, *str)) ++str;
	return str;
}

char *str_split(char **str, const char *delim) {
	char *begin;
	if (*str == NULL || **str == '\0') return NULL;
	for (begin = *str; *str && !strchr(delim, **str); ++*str) ;
	if (**str != '\0') { **str = '\0'; ++*str; }
	return begin;
}


/*============================================================================*/
char *next_token(char **str) {
	if (*str == NULL) return NULL;
	*str = str_skip(*str, " \v\t");
	switch (**str) {
		case '\0': case '#': return NULL;
		case '"': ++*str; return str_split(str, "\"");
		default: return str_split(str, " \v\t");
	}
}


char *read_line(const char *fmt, ...) {
	static char buffer[256];
	char *line;
	if (fmt != NULL) {
		va_list va;
		va_start(va, fmt);
		vprintf(fmt, va);
		va_end(va);
		fflush(stdout);
	}
	if ((line = fgets(buffer, sizeof(buffer), stdin)) == NULL) return NULL;
	line = str_skip(line, " \v\t");
	line = str_split(&line, "\r\n");
	return line ? line : "";
}


/*============================================================================*/
void free_variable(Variable *var) {
	while (var) {
		Variable *next = var->next;
		str_free(var->name);
		str_free(var->data);
		free(var);
		var = next;
	}
}


char *set_var(const char *name, const char *fmt, ...) {
	Variable *var;

	for (var = variables; var; var = var->next) {
		if (!strcasecmp(var->name, name)) break;
	}

	if (fmt) {
		va_list va;
		char buffer[1024];

		va_start(va, fmt);
		vsnprintf(buffer, sizeof(buffer), fmt, va);
		va_end(va);

		if (var == NULL) {
			if ((var = malloc(sizeof(Variable))) == NULL) panic("cannot allocate new variable");
			var->next = variables;
			var->name = str_copy((char*)name);
			var->data = str_copy(buffer);
			variables = var;
		} else {
			str_free(var->data);
			var->data = str_copy(buffer);
		}
	}

	return var ? var->data : NULL;
}


/*============================================================================*/
Selector *new_selector() {
	Selector *new = malloc(sizeof(Selector));
	if (new == NULL) panic("cannot allocate new selector");
	new->next = NULL;
	new->index = 0;
	return new;
}

void free_selector(Selector *sel) {
	while (sel) {
		Selector *next = sel->next;
		str_free(sel->name);
		str_free(sel->host);
		str_free(sel->port);
		str_free(sel->path);
		free(sel);
		sel = next;
	}
}


Selector *copy_selector(Selector *sel) {
	Selector *new = new_selector();
	new->next = NULL;
	new->index = 1;
	new->type = sel->type;
	new->name = str_copy(sel->name);
	new->host = str_copy(sel->host);
	new->port = str_copy(sel->port);
	new->path = str_copy(sel->path);
	return new;
}


Selector *append_selector(Selector *list, Selector *sel) {
	if (list == NULL) {
		sel->next = NULL;
		sel->index = 1;
		return sel;
	} else {
		Selector *it;
		for (it = list; it->next; it = it->next) ;
		it->next = sel;
		sel->next = NULL;
		sel->index = it->index + 1;
		return list;
	}
}


Selector *prepend_selector(Selector *list, Selector *sel) {
	sel->next = list;
	sel->index = list ? list->index + 1 : 1;
	return sel;
}


Selector *find_selector(Selector *list, const char *line) {
	int index;
	if ((index = atoi(line)) <= 0) return NULL;
	for (; list; list = list->next) if (list->index == index) return list;
	return NULL;
}


char *print_selector(Selector *sel, int with_prefix) {
	static char buffer[1024];
	if (sel == NULL) return "";
	snprintf(buffer, sizeof(buffer), "%s%s:%s/%c%s",
		with_prefix ? "gopher://" : "",
		sel->host, sel->port, sel->type, sel->path
	);
	return buffer;
}


Selector *parse_selector(char *str) {
	char *p;
	Selector *sel;

	sel = new_selector();
	sel->type = '1';

	if ((p = strstr(str, "gopher://")) == str) str += 9; /* skip "gopher://" */
	if ((p = strpbrk(str, ":/")) != NULL) {
		if (*p == ':') {
			sel->host = str_copy(str_split(&str, ":"));
			sel->port = str_copy(str_split(&str, "/"));
		} else {
			sel->host = str_copy(str_split(&str, "/"));
			sel->port = str_copy("70");
		}
		if (*str) sel->type = *str++;
		sel->path = str_copy(str);
	} else {
		sel->host = str_copy(str);
		sel->port = str_copy("70");
		sel->path = str_copy("");
	}

	sel->name = str_copy(print_selector(sel, 1));

	return sel;
}


Selector *parse_selector_list(char *str) {
	char *line;
	Selector *list = NULL, *sel;

	while ((line = str_split(&str, "\r\n")) != NULL) {
		if (*line == '\0' || *line == '.') break;

		sel = new_selector();
		list = append_selector(list, sel);

		sel->type = *line++;
		sel->name = str_copy(str_split(&line, "\t"));
		sel->path = str_copy(str_split(&line, "\t"));
		sel->host = str_copy(str_split(&line, "\t"));
		sel->port = str_copy(str_split(&line, "\t"));

		str = str_skip(str, "\r\n");
	}

	return list;
}


/*============================================================================*/
char *download(Selector *sel, const char *query, size_t *length) {
	struct addrinfo hints, *result, *it;
	char request[1024], *data;
	size_t total;
	int fd, received;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	if (getaddrinfo(sel->host, sel->port, &hints, &result) || result == NULL) {
		error("cannot resolve hostname `%s`", sel->host);
		goto fail;
	}

	for (it = result; it; it = it->ai_next) {
		if ((fd = socket(it->ai_family, it->ai_socktype, it->ai_protocol)) == -1) continue;
		if (connect(fd, it->ai_addr, it->ai_addrlen) == 0) break;
		close(fd); fd = -1;
	}

	freeaddrinfo(result);

	if (fd == -1) {
		error("cannot connect to `%s`:`%s`", sel->host, sel->port);
		goto fail;
	}

	if (query) snprintf(request, sizeof(request), "%s\t%s\r\n", sel->path, query);
	else snprintf(request, sizeof(request), "%s\r\n", sel->path);
	send(fd, request, strlen(request), 0);

	for (total = 0L, data = NULL;;) {
		if ((data = realloc(data, total + (1024 * 64))) == NULL) panic("cannot allocate download data");
		if ((received = recv(fd, &data[total], 1024 * 64, 0)) <= 0) break;
		total += received;
	}

	close(fd);
	data = realloc(data, total);
	data[total] = '\0';

	if (length) *length = total;
	return data;

fail:
	if (length) *length = 0L;
	return NULL;
}


char *download_to_temp(Selector *sel) {
	static char filename[1024];
	size_t length;
	char *data;
	int fd;

	if ((data = download(sel, NULL, &length)) == NULL) return NULL;
	snprintf(filename, sizeof(filename), "%s", "/tmp/delve.XXXXXXXX");
	if ((fd = mkstemp(filename)) == -1) {
		free(data);
		error("cannot create temporary file: %s", strerror(errno));
		return NULL;
	}
	write(fd, data, length);
	close(fd);
	free(data);
	return filename;
}


void download_to_file(Selector *sel) {
	char *filename, *def, *data;
	size_t length;
	FILE *fp;

	def = strrchr(sel->path, '/');
	if (*def == '/') ++def;

	if ((data = download(sel, NULL, &length)) == NULL) return;
	if ((filename = read_line("enter filename (press ENTER for `%s`): ", def)) == NULL) return;
	if (!strlen(filename)) filename = def;
	if ((fp = fopen(filename, "wb")) == NULL) {
		free(data);
		error("cannot create file `%s`: %s", filename, strerror(errno));
		return;
	}
	fwrite(data, 1, length, fp);
	fclose(fp);
	free(data);
}


Selector *download_to_menu(Selector *sel, const char *query) {
	char *data;
	Selector *list;

	if ((data = download(sel, query, NULL)) == NULL) return NULL;
	list = parse_selector_list(data);
	free(data);
	return list;
}


/*============================================================================*/
const char *find_selector_handler(char type) {
	char name[32];
	snprintf(name, sizeof(name), "type_%c", type);
	return set_var(name, NULL);
}


void print_menu(Selector *list, const char *filter) {
	for (; list; list = list->next) {
		if (filter && !strcasestr(list->name, filter) && !strcasestr(list->path, filter)) continue;
		switch (list->type) {
			case 'i': printf("     | %.76s\n", list->name); break;
			case '3': printf("     | \33[31m%.76s\33[0m\n", list->name); break;
			default:
				if (strchr("145679", list->type) || find_selector_handler(list->type)) {
					printf("%4d | \33[36;4m%.76s\33[0m\n", list->index, list->name);
				} else {
					printf("%4d | \33[36;0m%.76s\33[0m\n", list->index, list->name);
				}
				break;
		}
	}
}


void execute_handler(const char *handler, Selector *to) {
	char command[1024], *filename = NULL;
	size_t l;

	for (l = 0; *handler && l < sizeof(command) - 1; ) {
		if (handler[0] == '%' && handler[1] != '\0') {
			const char *append = "";
			switch (handler[1]) {
				case '%': append = "%"; break;
				case 'h': append = to->host; break;
				case 'p': append = to->port; break;
				case 's': append = to->path; break;
				case 'n': append = to->name; break;
				case 'f':
					if (filename == NULL) filename = download_to_temp(to);
					if (filename == NULL) return;
					append = filename;
					break;
			}
			handler += 2;
			while (*append && l < sizeof(command) - 1) command[l++] = *append++;
		} else command[l++] = *handler++;
	}
	command[l] = '\0';

	system(command);
	if (filename) remove(filename);
}


void navigate(Selector *to) {
	const char *query = NULL, *handler;

	switch (to->type) {
		case '7': /* gopher full-text search */
			query = read_line("enter gopher search string: ");
			/* fallthrough */
		case '1': { /* gopher submenu */
			Selector *new = download_to_menu(to, query);
			if (new == NULL) break;
			if (history != to) history = prepend_selector(history, copy_selector(to));
			free_selector(menu);
			print_menu(new, NULL);
			menu = new;
			break;
		}
		case '4': case '5': case '6': case '9': /* binary files */
			download_to_file(to);
			break;
		case 'i': case '3': /* ignore these selectors */
			break;
		default: /* try to invoke handler */
			if ((handler = find_selector_handler(to->type)) != NULL) {
				execute_handler(handler, to);
			} else {
				error("no handler for type `%c`", to->type);
			}
			break;
	}
}


/*============================================================================*/
static const Help gopher_help[] = {
	{
		"authors",
		"Credit goes to the following people:\n\n" \
		"Sebastian Steinhauer <s.steinhauer@yahoo.de>\n" \
	},
	{
		"back",
		"Syntax:\n" \
		"    BACK\n" \
		"\n" \
		"Description:\n" \
		"    Go back in history.\n" \
	},
	{
		"bookmarks",
		"Syntax:\n" \
		"    BOOKMARKS [<item-id>]\n" \
		"\n" \
		"Description:\n" \
		"    Show all defined bookmarks.\n" \
		"    If <item-id> is specified, navigate to the given <item-id> from bookmarks.\n" \
		"\n\n" \
		"Syntax:\n" \
		"    BOOKMARKS <name> <url>\n" \
		"\n" \
		"Description:\n" \
		"    Define a new bookmark with the given <name> and <url>.\n" \
	},
	{
		"commands",
		"available commands\n" \
		"back          bookmarks     help          history       open\n" \
		"quit          save          see           set           show\n" \
	},
	{
		"help",
		"Syntax:\n" \
		"    HELP [<topic>]\n" \
		"\n" \
		"Description:\n" \
		"    Show all help topics or the help text for a specific <topic>.\n" \
	},
	{
		"history",
		"Syntax:\n" \
		"    HISTORY [<item-id>]\n" \
		"\n" \
		"Description:\n" \
		"    Show the current history.\n" \
		"    If <item-id> is specified, navigate to the given <item-id> from history.\n" \
	},
	{
		"license",
		"delve - a simple terminal gopher client\n" \
		"Copyright (C) 2019  Sebastian Steinhauer\n" \
		"\n" \
		"This program is free software: you can redistribute it and/or modify\n" \
		"it under the terms of the GNU General Public License as published by\n" \
		"the Free Software Foundation, either version 3 of the License, or\n" \
		"(at your option) any later version.\n" \
		"\n" \
		"This program is distributed in the hope that it will be useful,\n" \
		"but WITHOUT ANY WARRANTY; without even the implied warranty of\n" \
		"MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n" \
		"GNU General Public License for more details.\n" \
		"\n" \
		"You should have received a copy of the GNU General Public License\n" \
		"along with this program.  If not, see <https://www.gnu.org/licenses/>.\n" \
	},
	{
		"open",
		"Syntax:\n" \
		"    OPEN <url>\n" \
		"\n" \
		"Description:\n" \
		"    Opens the given <url> as a gopher menu.\n" \
	},
	{
		"quit",
		"Syntax:\n" \
		"    QUIT\n" \
		"\n" \
		"Description:\n" \
		"    Quit the gopher client.\n"
	},
	{
		"save",
		"Syntax:\n" \
		"    SAVE <item-id>\n" \
		"\n" \
		"Description:\n" \
		"    Saves the given <item-id> from the menu to the disk.\n" \
		"    You will be asked for a filename.\n" \
	},
	{
		"see",
		"Syntax:\n" \
		"    SAVE <item-id>\n" \
		"\n" \
		"Description:\n" \
		"    Show the full gopher URL for the menu selector id.\n" \
	},
	{
		"set",
		"Syntax:\n" \
		"    SET [<name>] [<value>]\n" \
		"\n" \
		"Description:\n" \
		"    If no <name> is given it will show all variables.\n" \
		"    When <name> is given it will show this specific variable.\n" \
		"    If <data> is specified the variable will have this value.\n" \
		"    When the variable does not exist the variable will be created.\n" \
	},
	{
		"show",
		"Syntax:\n" \
		"    SHOW [<filter>]\n" \
		"\n" \
		"Description:\n" \
		"    Show the current gopher menu. If a <filter> is specified, it will\n" \
		"    show all selectors containing the <filter> in name or path.\n"
	},
	{
		"variables",
		"Type Handlers: type_X\n\n" \
		"     Variables with that name will define a command which will be\n" \
		"     executed for the selector type <X>.\n" \
		"     Example: set type_0 \"less %f\" # will launch `less`\n" \
		"     Format specifiers:\n" \
		"         %h - hostname\n" \
		"         %p - port\n" \
		"         %s - selector\n" \
		"         %n - name\n" \
		"         %f - temporary file (selector will be downloaded)\n" \
		"         %% - escape %\n" \
		"\n" \
		"Aliases: alias_XXXX\n\n" \
		"     Define an command alias.\n" \
		"     Example: set alias_b \"back\"\n" \
		"     Will introduce the command `b` which executes `back`.\n" \
	},
	{ NULL, NULL }
};


/*============================================================================*/
static void cmd_quit(char *line) {
	(void)line;
	exit(EXIT_SUCCESS);
}


static void cmd_open(char *line) {
	Selector *to = parse_selector(next_token(&line));
	navigate(to);
	free_selector(to);
}


static void cmd_show(char *line) {
	print_menu(menu, next_token(&line));
}


static void cmd_save(char *line) {
	Selector *to = find_selector(menu, line);
	if (to) download_to_file(to);
}


static void cmd_back(char *line) {
	Selector *to = history ? history->next : NULL;
	(void)line;
	if (to != NULL) {
		history->next = NULL;
		free_selector(history);
		history = to;
		navigate(to);
	} else {
		error("history empty");
	}
}


static void cmd_help(char *line) {
	int i;
	const Help *help;
	char *topic = next_token(&line);

	if (topic) {
		for (help = gopher_help; help->name; ++help) {
			if (!strcasecmp(help->name, topic)) {
				if (help->text) puts(help->text);
				else printf("sorry topic `%s` has no text yet :(\n", topic);
				return;
			}
		}
	}

	puts("available topics, type `help <topic>` to get more information");
	for (i = 1, help = gopher_help; help->name; ++help, ++i) {
		printf("%-13s ", help->name);
		if (i % 5 == 0) puts("");
	}
	puts("");
}


static void cmd_history(char *line) {
	Selector *to = find_selector(history, line);
	if (to != NULL) navigate(to);
	else print_menu(history, next_token(&line));
}


static void cmd_bookmarks(char *line) {
	Selector *to = find_selector(bookmarks, line);
	if (to != NULL) navigate(to);
	else {
		char *name = next_token(&line);
		char *url = next_token(&line);
		if (url) {
			Selector *sel = parse_selector(url);
			str_free(sel->name);
			sel->name = str_copy(name);
			bookmarks = append_selector(bookmarks, sel);
		} else print_menu(bookmarks, name);
	}
}


static void cmd_set(char *line) {
	char *name = next_token(&line);
	char *data = next_token(&line);

	if (name != NULL) {
		if (data) set_var(name, "%s", data);
		else puts(set_var(name, NULL));
	} else {
		Variable *it;
		for (it = variables; it; it = it->next) printf("%s = \"%s\"\n", it->name, it->data);
	}
}


static void cmd_see(char *line) {
	Selector *to = find_selector(menu, line);
	if (to && !strchr("3i", to->type)) puts(print_selector(to, 1));
}


static const Command gopher_commands[] = {
	{ "quit", cmd_quit },
	{ "open", cmd_open },
	{ "show", cmd_show },
	{ "save", cmd_save },
	{ "back", cmd_back },
	{ "help", cmd_help },
	{ "history", cmd_history },
	{ "bookmarks", cmd_bookmarks },
	{ "set", cmd_set },
	{ "see", cmd_see },
	{ NULL, NULL }
};


/*============================================================================*/
void eval(char *str, const char *filename) {
	static int nested =  0;
	const Command *cmd;
	char *line, *token, *alias, name[256];
	int line_no;

	if (nested >= 10) {
		error("eval() nested too deeply");
		return;
	} else ++nested;

	for (line_no = 1; (line = str_split(&str, "\r\n")) != NULL; ++line_no) {
		if ((token = next_token(&line)) != NULL) {
			for (cmd = gopher_commands; cmd->name; ++cmd) {
				if (!strcasecmp(cmd->name, token)) {
					cmd->func(line);
					break;
				}
			}
			if (cmd->name == NULL) {
				snprintf(name, sizeof(name), "alias_%s", token);
				if ((alias = set_var(name, NULL)) != NULL) {
					alias = str_copy(alias);
					eval(alias, name);
					str_free(alias);
				} else {
					if (filename) error("unknown command `%s` in file `%s` at line %d", token, filename, line_no);
					else error("unknown command `%s`", token);
				}
			}
		}
		str = str_skip(str, "\r\n");
	}

	--nested;
}


void shell() {
	char *line;
	Selector *to;
	while ((line = read_line("(\33[35m%s\33[0m)> ", print_selector(history, 0))) != NULL) {
		if ((to = find_selector(menu, line)) != NULL) navigate(to);
		else eval(line, NULL);
	}
}


/*============================================================================*/
void load_config_file(const char *filename) {
	long length;
	FILE *fp = NULL;
	char *data = NULL;

	if ((fp = fopen(filename, "rb")) == NULL) goto fail;
	if (fseek(fp, 0, SEEK_END)) goto fail;
	if ((length = ftell(fp)) <= 0) goto fail;
	if (fseek(fp, 0, SEEK_SET)) goto fail;
	if ((data = malloc(length + 1)) == NULL) goto fail;
	if (fread(data, 1, length, fp) != (size_t)length) goto fail;
	fclose(fp);
	data[length] = '\0';

	eval(data, filename);
	free(data);
	return;

fail:
	if (data) free(data);
	if (fp) fclose(fp);
}


void load_config_files() {
	char buffer[1024], *home;

	load_config_file("/etc/delve.conf");
	load_config_file("/usr/local/etc/delve.conf");
	if ((home = getenv("HOME")) != NULL) {
		snprintf(buffer, sizeof(buffer), "%s/.delve.conf", home);
		load_config_file(buffer);
	}
	load_config_file("delve.conf");
}


void quit_client() {
	free_variable(variables);
	free_selector(bookmarks);
	free_selector(history);
	free_selector(menu);
	puts("\33[0m");
}


int main(int argc, char **argv) {
	atexit(quit_client);
	(void)argc; (void)argv;

	puts(
		"delve - 0.5.4  Copyright (C) 2019  Sebastian Steinhauer\n" \
		"This program comes with ABSOLUTELY NO WARRANTY; for details type `help license'.\n" \
		"This is free software, and you are welcome to redistribute it\n" \
		"under certain conditions; type `help license' for details.\n" \
		"\n" \
		"Type `help` for help.\n" \
	);

	load_config_files();
	shell();

	return 0;
}
/* vim: set ts=4:sw=4 noexpandtab */
