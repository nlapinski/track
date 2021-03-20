#include "SDL.h"
#include "../include/tracker.h"
#include "SDL_image.h"
#include <signal.h>
#include <pthread.h>
#include <time.h>

/* mraa header */
#include "mraa/spi.h"

#define FONT_CW 12
#define FONT_CH 12
#define SSEQ_TRACKS 16

//lots of globals @_@
static int sdlflags = SDL_SWSURFACE |SDL_NOFRAME;//SDL_HWPALETTE	| SDL_HWSURFACE | SDL_FULLSCREEN | SDL_DOUBLEBUF;// | SDL_ASYNCBLIT	;	/* SDL display init flags */


int die =0;
int last_tick; //last sdl tick
int playing=0;
static int tempo =120;
static int pos=0;
static int track = 0;
static int emode =0;
// gui stufff
static SDL_Surface *font = NULL;
static SDL_Surface *screen = NULL;

static char tracks[SSEQ_TRACKS][2000];


#define snprintf_nowarn(...) (snprintf(__VA_ARGS__) < 0 ? abort() : (void)0)



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
	font = gui_load_image("font_1.5x.png");
	if(!font)
	{
		fprintf(stderr, "Couldn't load font!\n");
		return -1;
	}
	SDL_EnableKeyRepeat(1, 1);
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
		switch(a)
		{
		  case 0:
		  	break;

		  case '\e':
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
	SDL_FreeSurface(tmp);

}


void gui_box(int x, int y, int w, int h, Uint32 c, SDL_Surface *dst)
{
	SDL_Rect r;
	r.x = x;
	r.y = y;
	r.w = w;
	r.h = 1;
	SDL_FillRect(dst, &r, c);

	r.x = x;
	r.y = y + h - 1;
	r.w = w;
	r.h = 1;
	SDL_FillRect(dst, &r, c);

	r.x = x;
	r.y = y + 1;
	r.w = 1;
	r.h = h - 2;
	SDL_FillRect(dst, &r, c);

	r.x = x + w - 1;
	r.y = y + 1;
	r.w = 1;
	r.h = h - 2;
	SDL_FillRect(dst, &r, c);

	r.x = x;
	r.y = y;
	r.w = w;
	r.h = h;
//	gui_dirty(&r);
}


void gui_bar(int x, int y, int w, int h, Uint32 c, SDL_Surface *dst)
{
	SDL_Rect r;
	r.x = x;
	r.y = y;
	r.w = w;
	r.h = h;
	SDL_FillRect(dst, &r, SDL_MapRGB(dst->format, 0, 90, 0));
	gui_box(x, y, w, h, c, dst);
}

void gui_rec()
{

	if(emode){
		gui_text(220, 32, "**REC**", screen,255,0,199);
	}
	else{
		gui_text(220, 32, "**REC**", screen,9,255,199);
	}
}


void gui_songpos(int v)
{
	char buf[32];
	snprintf(buf, sizeof(buf), "SongPos: %4.1d", v);
	gui_text(220+220, 32, buf, screen,176, 82, 121);
}



void gui_tempo(int v)
{
	char buf[32];
	snprintf(buf, sizeof(buf), "  Tempo: %4.1d", v);
	gui_text(12, 32, buf, screen,9,255,199);
}
void gui_songedit(int pos, int ppos, int track, int editing)
{
	int t, n;
	//char buf[128];
	char np[5];
	char note[4];

	const int y0 = 64;

	/* Track names + cursor */
	gui_text((FONT_CW*4), y0 - FONT_CH,"T-0  T-1  T-2  T-3  T-4  T-5  T-6  T-7  T-8  T-9  T-A  T-B  T-C  T-D  T-E  T-F", screen,200,255,199);

	/* Notes */

	for(t = 0; t < SSEQ_TRACKS; ++t){

		//if(playing && tracks[t][pos]){
			//printf("note on ->%c  track ->%d\n",tracks[t][pos],t);
		//};

		for(n = 0; n < 32; ++n)
		{

			if(tracks[t][pos+n]){
				note[0]=tracks[t][pos+n];
				note[1]=tracks[t][pos+n];
				note[2]=tracks[t][pos+n];
				note[3]=tracks[t][pos+n];
			}
			else{
				note[0]='-';
				note[1]='-';
				note[2]='-';
				note[3]='-';
			}


			if(pos+n-16>-1){
			//n % m == n & (m - 1)
			//bitwise and to replace modulo
				if((pos+n)&(16-1)){
				gui_text(FONT_CW*3 + FONT_CW * (1 + t*5),y0 + FONT_CH * (1 + n) + 3,note, screen,9,255,199);
				}
				else{
				gui_text(FONT_CW*3 + FONT_CW * (1 + t*5),y0 + FONT_CH * (1 + n) + 3,note, screen,9,055,199);
				}
				if(t==0){
				//snprintf(np,5,"%04d",n);
				snprintf_nowarn(np, sizeof(np), "%.04d", pos+n-16);
				//sprintf(np, "%04d", n+pos-16);
				gui_text((0 + FONT_CW * (1 + t*5))-FONT_CW,y0 + FONT_CH * (1 + n) + 3,np, screen,0,97,77);
				//clear

				}
				memset(note, 0, sizeof note);
				memset(np, 0, sizeof np);
			}
		}
	}
	/* Cursors */
	gui_text((FONT_CW*5 * ( 1+(ppos )))-10, y0, "****", screen,9,255,199);
	gui_text((FONT_CW*5 * ( 1+(ppos )))-17,y0 + FONT_CH * (SSEQ_TRACKS + 1) ,"(   )", screen,9,255,199);
}




static void draw_main(void)
{
	Uint32 fwc = SDL_MapRGB(screen->format, 0, 0, 128);

	/* Clear */
	SDL_FillRect(screen, NULL, SDL_MapRGB(screen->format, 0, 20, 20));

	/* Song info panel */
	//gui_bar(6, 46, 228, 44, fwc, screen);
	gui_tempo(tempo);
	gui_songpos(pos);
	gui_rec();
	/* Status box */
	//gui_bar(6, 94, 228, 44, fwc, screen);
	//gui_status(0, 0, 0);

	/* Song editor */
	//gui_bar(6, 142, 640 - 12, FONT_CH * (SSEQ_TRACKS + 2) + 12,
	//		fwc, screen);
	gui_songedit(pos, track, 0, 0);

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
	  case SDLK_LEFT:
	  	track--;
	  	if(track<0){
	  		track=15;
	  	}
		break;
	  case SDLK_RIGHT:
	  	track++;
	  	if(track>15){
	  		track=0;
	  	}
		break;
	  case SDLK_UP:
	  	pos--;
		break;
	  case SDLK_DOWN:
	  	pos++;
		break;
	  case SDLK_INSERT:
	  case SDLK_SPACE:
	  		playing=!playing;
	  		break;
	  case SDLK_i:
	  	emode=!emode;
		break;
	  case SDLK_RETURN:
	  	pos=0;
	  	playing=1;
		break;
	  case SDLK_x:
		break;
	  case SDLK_v:
		break;
	  case SDLK_o:
		break;
	  case SDLK_s:
		break;
	  case SDLK_1:
	  	if(emode){
  			tracks[track][pos+16]='1';
  			//printf("%c \n",tracks[track][pos+16]);
  			if(!playing){
  			pos+=1;
  			}
	  	}
		break;
		case SDLK_d:
		if(emode){
			tracks[track][pos+16]='-';
			//printf("%c \n",tracks[track][pos+16]);
				if(!playing){
					pos+=1;
				}
			}
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

void sleep_us(unsigned long microseconds)
{
    struct timespec ts;
    ts.tv_sec = microseconds /1000000;             // whole seconds
    ts.tv_nsec = (microseconds % 1000000) * 1000;    // remainder, in nanoseconds
    nanosleep(&ts, NULL);
}

void *step_clock(void *arg)
{
    while(1)
    {
    	//make this clock monotonic to try and fix any jitter///also output mraa clock pulse here
    	if(playing){
    		pos+=1;
    		//60,000 / 100 bpm = 600ms /8 for 32nd notes
    		//long time = ;
    		//printf("%d\n", time);

    	}
        	sleep_us((unsigned long)((60000 / tempo)/8)*1000);

    }
    return 0;
}


int main(int argc, char *argv[])
{

	//spawn clock threads
	pthread_t tid;
	pthread_create(&tid, NULL, &step_clock, NULL);


	///init SDL
	if(SDL_Init(0) < 0)
		return -1;

	atexit(SDL_Quit);
	signal(SIGTERM, breakhandler);
	signal(SIGINT, breakhandler);

	//SDL_SetRefreshRate(75);
	//2880x1800 ???
	screen = SDL_SetVideoMode(1024,600,8, sdlflags);
	if(!screen)
	{
		fprintf(stderr, "Couldn't open display!\n");
		SDL_Quit();
		return -1;
	}
	SDL_ShowCursor(0);
	SDL_WM_SetCaption("tracker", "tracker");
	//end sdl stuff

	//start up gui
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
	last_tick = SDL_GetTicks();
	while(!die)
	{
		if(pos>1998){
			pos=0;
		}


		if(pos<0){
			pos=0;
		}
		SDL_Event ev;
		int tick = SDL_GetTicks();
		int dt = tick - last_tick;
		last_tick = tick;

		//gui_text( 390, tick/100, "abcdefghijklmnopqrstuvwxyz", screen,(23+34+255+tick)%255,(255-255-1212-tick)%255,tick%255);
		//printf("%d\n",playing);

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


		//SDL_UpdateRect(screen, 0, 0, 0, 0);
		draw_main();
		SDL_Flip(screen);
		//sdl wait
		SDL_Delay(15);
	}

	SDL_FreeSurface(font);
	font = NULL;
	SDL_Quit();
	return 0;
}
