/******
Demo for ssd1306 i2c driver for  Raspberry Pi
******/
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include "st7735.h"
#include "message.h"
#include "time.h"
#include <unistd.h>

/* How often the page is refreshed. This also sets the interval the CPU
   load is averaged over, since that is measured between reads. */
#define REFRESH_SECONDS 2

static volatile sig_atomic_t running = 1;

static void request_stop(int signum)
{
	(void)signum;
	running = 0;
}

int main(void)
{
	struct sigaction action;
	char shown[DISPLAY_MESSAGE_MAX] = {0};
	char current[DISPLAY_MESSAGE_MAX] = {0};

	if(lcd_begin())      //LCD Screen initialization
	{
		return 1;
	}

	/* systemd stops this service with SIGTERM. Without a handler the
	   process dies mid-frame, leaving a stale reading on a display nothing
	   is updating any more, and the i2c descriptor open. Catching it lets
	   the screen be cleared on the way out.

	   sa_flags stays zero, so SA_RESTART is off and the sleep below is
	   interrupted by delivery. A stop therefore takes effect at once
	   rather than after the current refresh interval elapses. */
	memset(&action, 0, sizeof(action));
	action.sa_handler = request_stop;
	sigaction(SIGTERM, &action, NULL);
	sigaction(SIGINT, &action, NULL);

	sleep(1);
	while(running)
	{
		/* Every reading is on screen at once, so there is nothing to
		   rotate through; this repaints whichever of them changed. */
		lcd_display_page();
		/* The header reflects whatever display-cli had set by the time the
		   page was drawn. */
		display_message_read(shown, sizeof(shown));

		sleep(REFRESH_SECONDS);
		if(!running)
		{
			break;
		}

		/* Only the header is redrawn on a message change, so the readings
		   on display are left alone. */
		display_message_read(current, sizeof(current));
		if(strcmp(current, shown) != 0)
		{
			lcd_display_header();
		}
	}

	lcd_fill_screen(ST7735_BLACK);
	lcd_end();
	return 0;
}
