#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <unistd.h>
#include <signal.h>

#include <X11/Xlib.h>
#include <X11/keysymdef.h>
#include <X11/XF86keysym.h>

struct binding {
	KeySym keysym;
	char **cmd;
};

#define B(sym, ...) { .keysym = sym, .cmd = (char *[]) { __VA_ARGS__, NULL } }

static struct binding bindings[] = {
	B(XF86XK_AudioRaiseVolume,	"amixer", "-q", "sset", "Master", "1+", "unmute"),
	B(XF86XK_AudioLowerVolume,	"amixer", "-q", "sset", "Master", "1-", "unmute"),
	B(XF86XK_AudioMute,		"amixer", "-q", "sset", "Master", "toggle"),

	B(XF86XK_AudioPlay,	"mpc", "-q", "toggle"),
	B(XF86XK_AudioStop,	"mpc", "-q", "stop"),
	B(XF86XK_AudioNext,	"mpc", "-q", "next"),
	B(XF86XK_AudioPrev,	"mpc", "-q", "prev"),

	B(XF86XK_MonBrightnessUp,   "xbacklight", "-inc", "+10"),
	B(XF86XK_MonBrightnessDown, "xbacklight", "-dec", "+10"),

	B(NoSymbol, NULL) // end
};

#undef B

static struct sigaction sachld = {
	.sa_handler = SIG_DFL,
	.sa_flags = SA_NOCLDWAIT
};

static void launch(char **cmd) {

	if (!cmd)
		return;

	switch (fork()) {
	case -1:
		perror("fork");
		break;
	case 0:
		setsid();
		switch (fork()) {
		case 0:
			if (execvp(cmd[0], cmd) == -1)
				perror("exec");
			break;
		case -1:
			perror("fork");
			break;
		}
		exit(0);
	default:
		break;
	}
}

static char** lookup_cmd(KeySym sym) {

	if(sym == NoSymbol)
		return NULL;

	for (struct binding *b = bindings; b->keysym != NoSymbol; b++) {
		if (sym == b->keysym)
			return b->cmd;
	}

	return NULL;
}

static void grab_keys(Display *dpy) {

	for (struct binding *b = bindings; b->keysym != NoSymbol; b++) {
		KeyCode code = XKeysymToKeycode(dpy, b->keysym);
		if (code) {
			XGrabKey(dpy, code, AnyModifier, DefaultRootWindow(dpy), False, GrabModeAsync, GrabModeAsync);
		} else {
			fprintf(stderr, "can't find keycode for keysym %s (%ld)\n", XKeysymToString(b->keysym), b->keysym);
		}
	}

}

static void ungrab_keys(Display *dpy) {
	XUngrabKey(dpy, AnyKey, AnyModifier, DefaultRootWindow(dpy));
}

void run(Display *dpy) {

	while (1) {
		XEvent e;

		XNextEvent(dpy, &e);

		switch (e.type) {
		case KeyRelease: /* fallthrough */
		case KeyPress: {
			KeySym sym = XLookupKeysym(&e.xkey, 0);

			printf("%s %d (%s)\n", e.type == KeyPress ? "press" : "release",
				 e.xkey.keycode, XKeysymToString(sym));

			if (e.type == KeyPress)
				launch(lookup_cmd(sym));
		} break;
		}
	}

}

static Display* init(void) {

	Display *dpy = NULL;

	if (sigaction(SIGCHLD, &sachld, NULL) == -1) {
		perror("sigaction");
		return dpy;
	}

	dpy = XOpenDisplay(NULL);
	if (!dpy) {
		fprintf(stderr, "can't open display\n");
	} else {
		grab_keys(dpy);
		XSync(dpy, False);
	}

	return dpy;
}

static void cleanup(Display *dpy) {
	if (dpy) {
		ungrab_keys(dpy);
		XCloseDisplay(dpy);
		dpy = NULL;
	}
}

static void print_bindings(void) {

	for (struct binding *b = bindings; b->keysym != NoSymbol; b++) {
		printf("%s =>", XKeysymToString(b->keysym));
		for (char **s = b->cmd; *s; s++)
			printf(" %s", *s);
		puts("");
	}
}

static void print_version(void) {
	puts("mkb " VERSION);
}

static void print_usage(const char* cmd) {
	printf(	"usage: %s [-h|-v|-b|-d]\n"
		"    -h    print help\n"
		"    -v    print version\n"
		"    -b    print bindings\n"
		"    -d    run as daemon\n",
		cmd);
}

int main(int argc, char *argv[]) {
	Display *dpy = NULL;
	int opt;
	int daemonize = 0;

	while ((opt = getopt(argc, argv, "hvbd")) != -1) {
		switch (opt) {
		case 'h':
			print_usage(argv[0]);
			return EXIT_SUCCESS;
		case 'b':
			print_bindings();
			return EXIT_SUCCESS;
		case 'v':
			print_version();
			return EXIT_SUCCESS;
		case 'd':
			daemonize = 1;
			break;
		default:
			print_usage(argv[0]);
			return EXIT_FAILURE;
		}
	}

	if (optind < argc) {
		print_usage(argv[0]);
		return EXIT_FAILURE;
	}

	dpy = init();

	if (dpy != NULL) {

		if (daemonize && (daemon(1, 0) == -1))
			perror("daemon");

		run(dpy);
		cleanup(dpy);

		return EXIT_SUCCESS;
	}

	return EXIT_FAILURE;
}
