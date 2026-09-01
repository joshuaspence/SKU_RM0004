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

/* Screens lcd_display() cycles through, and how long each one stays up. */
#define SCREEN_COUNT 4
#define SCREEN_DWELL_SECONDS 2

static volatile sig_atomic_t running = 1;

static void request_stop(int signum)
{
	(void)signum;
	running = 0;
}

int main(void)
{
	uint8_t symbol = 0;
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
	   rather than after the current screen finishes its dwell. */
	memset(&action, 0, sizeof(action));
	action.sa_handler = request_stop;
	sigaction(SIGTERM, &action, NULL);
	sigaction(SIGINT, &action, NULL);

	sleep(1);
	while(running)
	{
		lcd_display(symbol);
		/* The header now reflects whatever display-cli had set by the time
		   the screen was drawn. */
		display_message_read(shown, sizeof(shown));

		sleep(SCREEN_DWELL_SECONDS);
		if(!running)
		{
			break;
		}

		/* Pick up a change without waiting for the rotation to reach the
		   screen that repaints in full. Only the header is redrawn, so the
		   reading currently on display is left alone. */
		display_message_read(current, sizeof(current));
		if(strcmp(current, shown) != 0)
		{
			lcd_display_header();
		}

		symbol = (uint8_t)((symbol + 1) % SCREEN_COUNT);
	}

	lcd_fill_screen(ST7735_BLACK);
	lcd_end();
	return 0;
}
