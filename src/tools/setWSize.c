#include <sys/ioctl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h> /* memset strcmp */

#include <X11/Xlib.h>

int
convRowToPixel(int row)
{
	return (row - 1) * 15 + 19;
}

int
convColToPixel(int col)
{
	return (col - 1) * 9 + 32;
}

int main()
{
	struct winsize wsize = {37, 100, 0, 0};
	char *term = NULL;
	/* struct winsize wsize = {24, 80, 0, 0}; */

	/* environment variables : TERM */
	term = getenv("TERM");

	printf("trying to set PTY to size : col %d row %d pixels : %dx%d\n"
		,wsize.ws_col
		,wsize.ws_row 
		,wsize.ws_xpixel
		,wsize.ws_ypixel
		);

	ioctl(0, TIOCSWINSZ, &wsize);

	memset(&wsize, 0, sizeof(struct winsize));

	/* we fetch the window size for the current pty */
	ioctl(0, TIOCGWINSZ, &wsize);

	printf("PTY result -- size : col %d row %d pixels : %dx%d\n"
		,wsize.ws_col
		,wsize.ws_row 
		,wsize.ws_xpixel
		,wsize.ws_ypixel
		);

	printf("Terminal type : %s\n", term);

	if (!strcmp("xterm", term)) /* X11 pty */
	{
		Display *dpy = NULL;
		char *cwId = NULL;
		Window w = 0;
		unsigned int width = 0, height = 0;

		cwId = getenv("WINDOWID");

		dpy = XOpenDisplay(NULL);

		w = atoi(cwId);

		XResizeWindow(dpy, w, convColToPixel(wsize.ws_col), convRowToPixel(wsize.ws_row));
		
		{
			Window wroot;
			int x, y;
			unsigned int border, depth;

			XGetGeometry(dpy, w, &wroot, &x, &y, &width, &height, &border, &depth);

			printf("new size of the window : %dx%d -- got %dx%d\n", 
				convColToPixel(wsize.ws_col), 
				convRowToPixel(wsize.ws_row), 
				width, height);
		}

		XCloseDisplay(dpy);
	}

	return 0;
}
