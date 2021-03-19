#include "SDL.h"
#include "../include/tracker.h"
#include "SDL_image.h"


#define FONT_CW 8
#define FONT_CH 8
#define SSEQ_TRACKS 16

//lots of globals @_@
static int sdlflags = SDL_HWSURFACE| SDL_FULLSCREEN ;	/* SDL display init flags */


int die =0;	
int last_tick; //last sdl tick
static int tempo =120;

// gui stufff
static SDL_Surface *font = NULL;
static SDL_Surface *screen = NULL;


SDL_Surface *gui_load_image(const char *fn)
{


	int imgflags = IMG_INIT_PNG;
	IMG_Init(imgflags);
    SDL_Surface *img = IMG_Load(fn );

    SDL_Surface *tmp = SDL_DisplayFormat(img);

	if(!img)
		return NULL;

//    SDL_SetColorKey(img,SDL_SRCCOLORKEY,SDL_MapRGB(img->format,255,255,255));
//    SDL_FillRect(tmp,&tmp->clip_rect,SDL_MapRGB(tmp->format,9,255,199));
//	SDL_BlitSurface(img,NULL,tmp,NULL);
//	SDL_SetColorKey(tmp,SDL_SRCCOLORKEY,SDL_MapRGB(tmp->format,255,0,255));

	SDL_FreeSurface(img);
	return tmp;
}

int gui_open(SDL_Surface *scrn)
{
	//screen = scrn;
	font = gui_load_image("font.png");
	if(!font)
	{
		fprintf(stderr, "Couldn't load font!\n");
		return -1;
	}
	SDL_EnableKeyRepeat(250, 25);
	return 0;
}


void gui_text(int x, int y, const char *txt, SDL_Surface *dst,int r, int g ,int b)
{
    SDL_Surface *tmp = SDL_DisplayFormat(font);


    SDL_SetColorKey(font,SDL_SRCCOLORKEY,SDL_MapRGB(font->format,255,255,255));
    SDL_FillRect(tmp,&tmp->clip_rect,SDL_MapRGB(tmp->format,r,g,b));
	SDL_BlitSurface(font,NULL,tmp,NULL);
	SDL_SetColorKey(tmp,SDL_SRCCOLORKEY,SDL_MapRGB(tmp->format,255,0,255));


	//fuck...
	int remap[]={129,90,28,70,89,117,83,84,85,86,28,73,101,102,103,104,56,57,58,59,60,61,62,63,64,65,27,28,28,28,28,0,91,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,83,84,73,0,0,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57};
	int sx = x;
	int sy = y;
	const char *stxt = txt;
	SDL_Rect sr;
	sr.w = FONT_CW;
	sr.h = FONT_CH;
	while(*txt)
	{
		int c = *txt++;
		char a = remap[c-32];
		
		///ughhh
		//printf("%d=%d a=c\n",a,c-32);
		//c=convert;
		switch(c)
		{
		  case 0:	/* terminator */
			break;
		  case '\n':	/* newline */
			x = sx;
			y += FONT_CH;
			break;
		  case '\t':	/* tab */
			x -= sx;
			x += 8 * FONT_CW;
			x %= 8 * FONT_CW;
			x += sx;
			break;
		 
		  default:	/* printables */
		  {
			SDL_Rect dr;
			if(c < ' ' || c > 127)
				c = 127;
			//a -=32;
			c=a;
			sr.x = (c % (font->w / FONT_CW)) * FONT_CW;
			sr.y = (c / (font->w / FONT_CW)) * FONT_CH;
			dr.x = x;
			dr.y = y;
			SDL_BlitSurface(tmp, &sr, dst, &dr);
			//gui_dirty(&dr);
			x += FONT_CW;
			break;
		  }
		}
	}

	
}


void gui_tempo(int v)
{
	char buf[32];
	snprintf(buf, sizeof(buf), "  Tempo: %4.1d", v);
	gui_text(12, 52, buf, screen,9,255,199);
}
void gui_songedit(int pos, int ppos, int track, int editing)
{
	int t, n;
	char buf[128];
	SDL_Rect r;
	const int y0 = 146;

	/* Clear */
	r.x = 12 - 2;
	r.y = y0 - 2;
	r.w = FONT_CW * 38 + 4;
	r.h = FONT_CH * (SSEQ_TRACKS + 2) + 4 + 5;

	/* Upper time bar */
	snprintf(buf, sizeof(buf), "\027...\022...\022...\022..."
			"\027...\022...\022...\022...");
	gui_text(12 + 6 * FONT_CW, y0, buf, screen,9,255,199);

	/* Track names + cursor */
	gui_text(12, y0 + FONT_CH + 3, "Trk00\nTrk01\nTrk02\nTrk03\n"
			"Trk04\nTrk05\nTrk06\nTrk07\n"
			"Trk08\nTrk09\nTrk10\nTrk11\n"
			"Trk12\nTrk13\nTrk14\nTrk15",
			screen,9,255,199);
	gui_text(12, y0 + FONT_CH * (1 + track) + 3,
			"\003\001\005", screen,9,255,199);

	/* Lower time bar */
	snprintf(buf, sizeof(buf), "\007%.4d\022...\007%.4d\022..."
			"\007%.4d\022...\007%.4d\022...",
			pos, pos + 8, pos + 16, pos + 24);
	gui_text(12 + 6 * FONT_CW,
			y0 + FONT_CH * (SSEQ_TRACKS + 1) + 6, buf, screen,9,255,199);

	/* Notes */
	buf[1] = 0;
	for(t = 0; t < SSEQ_TRACKS; ++t)
		for(n = 0; n < 32; ++n)
		{
			//int note = sseq_get_note(pos + n, t);
			int note =35;
			if(note < 0)
				continue;
			else
				buf[0] = note;
			gui_text(12 + FONT_CW * (6 + n),
					y0 + FONT_CH * (1 + t) + 3,
					buf, screen,9,255,199);
		}

	/* Cursors */
	gui_text(12 + FONT_CW * (6 + (ppos & 0x1f)), y0, "\003", screen,9,255,199);
	gui_text(12 + FONT_CW * (6 + (ppos & 0x1f)),
			y0 + FONT_CH * (SSEQ_TRACKS + 1) + 6,
			"\003", screen,9,255,199);
	//if(editing)
	//	gui_text(12 + FONT_CW * (6 + (ppos & 0x1f)),
	//			y0 + FONT_CH * (1 + track) + 3,
	//			"\007", screen);
}



static void draw_main(void)
{
	Uint32 fwc = SDL_MapRGB(screen->format, 0, 128, 0);

	/* Clear */
	SDL_FillRect(screen, NULL, SDL_MapRGB(screen->format, 0, 20, 20));
	//gui_dirty(NULL);

	//logo(fwc);

	/* Oscilloscope frames */
	//gui_bar(240 - 2, 8 - 2, 192 + 4, 128 + 4, fwc, screen);
	//gui_bar(440 - 2, 8 - 2, 192 + 4, 128 + 4, fwc, screen);

	/* Song info panel */
	//gui_bar(6, 46, 228, 44, fwc, screen);
	gui_tempo(tempo);
	//gui_songpos(0);

	/* Status box */
	//gui_bar(6, 94, 228, 44, fwc, screen);
	//gui_status(0, 0, 0);

	/* Song editor */
	//gui_bar(6, 142, 640 - 12, FONT_CH * (SSEQ_TRACKS + 2) + 12,
	//		fwc, screen);
	gui_songedit(0, 0, 0, 0);

	/* Message bar */
	//gui_bar(6, screen->h - FONT_CH - 12 - 6,
	//		640 - 12, FONT_CH + 12, fwc, screen);
	//gui_message(NULL, -1);
}


//handle key control
static void handle_key_ctrl(SDL_Event *ev)
{
	switch(ev->key.keysym.sym)
	{
	  case SDLK_INSERT:
	  case SDLK_c:
		break;
	  case SDLK_x:
		break;
	  case SDLK_v:
		break;
	  case SDLK_o:
		break;
	  case SDLK_s:
		break;
	  case SDLK_n:
		break;
	  case SDLK_0:
	  	tempo++;
		break;
	  case SDLK_9:
	  	tempo--;
		break;
	  case SDLK_q:
		die=1;
		printf("clean user exit\n \n");
		break;
	  default:
		break;
	}
}



//handle program close
static void breakhandler(int a)
{
	die = 1;
}


int main(int argc, char *argv[])
{

	
	///init SDL
	//SDL_Surface *screen;

	if(SDL_Init(0) < 0)
		return -1;

	atexit(SDL_Quit);
	signal(SIGTERM, breakhandler);
	signal(SIGINT, breakhandler);

	//2880x1800
	screen = SDL_SetVideoMode(1440/2, 900/2	, 16, sdlflags);
	if(!screen)
	{
		fprintf(stderr, "Couldn't open display!\n");
		SDL_Quit();
		return -1;
	}
	SDL_WM_SetCaption("tracker", "tracker");
	//end sdl stuff

	if(gui_open(screen) < 0)
	{
		fprintf(stderr, "Couldn't start GUI!\n");
		SDL_Quit();
		return -1;
	}
		
	//test fonts
	//gui_text( 20, 10, "ABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890", screen,233,10,0);
	//gui_text( 20, 20, "!@#$%^&*()-=_+", screen,23,255,0);
	//gui_text( 20, 30, "()", screen,9,255,199);
	//gui_text( 20, 40, "}}}}{{{", screen,9,55,199);
	//gui_text( 20, 50, "[]", screen,255,25,199);
	//gui_text( 20, 60, "abcdefghijklmnopqrstuvwxyz", screen,9,255,199);



	//gui_text( 0, 2, "123456789qwertyuiopasdfghjklzxcv", screen);
	//gui_text( 0, 122, "123456789qwertyuiopasdfghjklzxcv", screen);
	last_tick = SDL_GetTicks();
	while(!die)
	{
		SDL_Event ev;
		int tick = SDL_GetTicks();
		int dt = tick - last_tick;
		last_tick = tick;
		SDL_UpdateRect(screen, 0, 0, 0, 0);
	    //SDL_FillRect(screen,&screen->clip_rect,SDL_MapRGB(screen->format,0,0,0));

		draw_main();
		//gui_text( 390, tick/100, "abcdefghijklmnopqrstuvwxyz", screen,(23+34+255+tick)%255,(255-255-1212-tick)%255,tick%255);

		/* Handle GUI events */
		while(SDL_PollEvent(&ev))
		{
			switch(ev.type)
			{
			  case SDL_KEYDOWN:
			  	handle_key_ctrl(&ev);
				break;
			  case SDL_QUIT:
				break;
			  default:
				break;
			}
		}

	
		//sdl wait
		SDL_Delay(10);
	}

	SDL_FreeSurface(font);
	font = NULL;
	SDL_Quit();
	return 0;
}

