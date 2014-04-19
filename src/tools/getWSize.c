#include <sys/ioctl.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h> /* memset strcmp */

#include <X11/Xlib.h>

int main()
{
	struct winsize wsize;
	char *term = NULL;
	/* we fetch the window size for the current pty */
	ioctl(0, TIOCGWINSZ, &wsize);


	printf("PTY size : col %d row %d pixels : %dx%d\n"
		,wsize.ws_col
		,wsize.ws_row 
		,wsize.ws_xpixel
		,wsize.ws_ypixel
		);

	term = getenv("TERM");

	if (!strcmp("xterm", term)) /* X11 pty */
	{
		Display *dpy = NULL;
		char *cwId = NULL;
		Window w = 0;
		unsigned int width = 0, height = 0;

		cwId = getenv("WINDOWID");

		dpy = XOpenDisplay(NULL);

		w = atoi(cwId);

		{
			Window wroot;
			int x, y;
			unsigned int border, depth;

			XGetGeometry(dpy, w, &wroot, &x, &y, &width, &height, &border, &depth);

			printf("size of the window : %dx%d\n", width, height);
		}

		XCloseDisplay(dpy);

	}

	return 0;
}
