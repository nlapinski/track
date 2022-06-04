//#include "SDL.h"
#include "../include/tracker.h"
//#include "SDL_image.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>

#include <signal.h>
#include <pthread.h>
#include <time.h>
//#include "midi_notes_1.h"
//#include "midi_notes_2.h"
/* mraa header */
#include "mraa/spi.h"

#include <stdio.h>
#include <stdlib.h>
FILE * f;

#define FONT_CW 12
#define FONT_CH 12
#define SSEQ_TRACKS 16

/* SPI declaration */
#define SPI_BUS 0

/* SPI frequency in Hz */
//15mhz
#define SPI_FREQ 15000000

//trying a slower speed 5mhz
//#define SPI_FREQ 5000000

//global spi context
mraa_spi_context spi;

//lots of globals @_@
//static int sdlflags = SDL_HWSURFACE |SDL_NOFRAME |SDL_DOUBLEBUF;//|SDL_OPENGLBLIT;//|SDL_OPENGLBLIT;//SDL_HWPALETTE	| SDL_HWSURFACE | SDL_FULLSCREEN | SDL_DOUBLEBUF;// | SDL_ASYNCBLIT	;	/* SDL display init flags */


int die =0;
int last_tick; //last sdl tick
int playing=0;
static int tempo =85;
static int pos=0;
static int track = 0;
static int emode =0;
// gui stufff
static SDL_Surface *font = NULL;
//static SDL_Surface *screen = NULL;


SDL_Window *screen;// = SDL_CreateWindow("tracker",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,1024, 600,SDL_WINDOW_FULLSCREEN | SDL_WINDOW_OPENGL);
SDL_Renderer *sdlRenderer;// = SDL_CreateRenderer(screen, -1, 0);
SDL_Texture* sdlTexture;// = SDL_CreateTexture(sdlRenderer,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,1024, 600);
SDL_Texture* font_tex;// SDL_CreateTexture(sdlRenderer,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STATIC,408,96);
extern Uint32 *myPixels;  // maybe this is a surface->pixels, or a malloc()'d buffer, or whatever.
//pixel format?

Uint32 format;// = SDL_GetWindowPixelFormat( gWindow );
SDL_PixelFormat* mappingFormat;// = SDL_AllocFormat( format );

static int cp_buffer[128];
static int cp_len=8;
static int tracks[SSEQ_TRACKS][3000];
static int arp_len=4;
int clear[16];
static int oct = 1;
int jump =0;

#define snprintf_nowarn(...) (snprintf(__VA_ARGS__) < 0 ? abort() : (void)0)



static void write_pin(mraa_spi_context spi,int pin,int val){

	uint8_t low = val & 0xff;
	uint8_t high=(val>>8) & 0xff;
	//uint8_t p1= 0x30 | pin;
	uint8_t pat[4];
	pat[0] =0x00;
	pat[1]=0x30 | pin;
	pat[2]=high;
	pat[3]=low;


	  //data.LT_int16 = dac_code;                              // Copy DAC code to union
	  //data_array[3] = 0;                                     // Only required for 32 byte readback transaction
	  //data_array[2] = dac_command | dac_address;             // Build command / address byte
	  //data_array[1] = data.LT_byte[1];                       // MS Byte
	  //data_array[0] = data.LT_byte[0];                       // LS Byte

	mraa_spi_write_buf(spi, pat, 4);


	//mraa_spi_write(spi,0x00);
	//mraa_spi_write(spi,0x30+pin);
	//mraa_spi_write(spi,high);
	//mraa_spi_write(spi,low);


}



int scales[15][12]={
  {
    1,0,1,0,1,1,0,1,0,1,0,1                        }
  , // a minor scale  A, B, C, D, E, F, and G.

  {
    1,0,1,1,0,1,0,1,1,0,1,0                        }
  , //C natural minor

  {
    1,1,1,1,1,1,1,1,1,1,1,1                        }
  , //chroma

  {
    1,0,1,0,1,1,0,1,0,1,0,1                        }
  , //maj dia F—C—G—D—A—E—B?
  {
    1,0,1,0,1,0,0,1,0,1,0,0                        }
  , //maj penta C, D, E, G, A.?

  {
    1,0,1,1,1,0,0,1,0,1,0,0                        }
  , //maj blues C, D, D♯/E♭, E, G, A
  //{1,0,1,0,1,0,1,0,1,0,1,0}, //whole tone
  {
    1,0,0,0,1,0,0,1,0,0,0,0                        }
  , //maj chord
  {
    1,0,0,0,0,0,0,1,0,0,0,0                        }
  , //maj fifth
  {
    1,0,0,0,1,0,0,1,0,0,1,0                        }
  , //maj +7

  {
    1,0,1,1,0,1,0,1,1,0,1,0                        }
  , //min dia
  {
    1,0,0,1,0,1,0,1,0,0,1,0                        }
  , //min penta
  {
    1,0,0,1,0,1,1,1,0,0,1,0                        }
  , //min blues
  {
    1,0,0,1,0,0,0,1,0,0,0,0                        }
  , //min chord
  {
    1,0,1,0,1,0,1,0,1,0,1,0                        }
  , //whole tone scale
  {
    1,0,0,1,0,0,0,1,0,0,1,0                        }
  , //min +7
};
uint8_t quantizeNote(uint8_t _scale,uint8_t note){
  if(scales[_scale][note%12]){
    return note;
  }
  else{
    uint8_t higher=0;
    while(!scales[_scale][(note+higher)%12]){
      higher++;
    }
    uint8_t lower=0;
    while(!scales[_scale][(note-lower)%12]){
      lower++;
    }
    if(higher<lower) return note+higher;
    else return note-lower;
  }
  return note;
}



struct timespec deadline;


void sleep_us(unsigned long microseconds)
{
		//struct timespec deadline;
		clock_gettime(CLOCK_MONOTONIC, &deadline);
		//clock_gettime(CLOCK_REALTIME, &(deadline));

		// Add the time you want to sleep
		deadline.tv_nsec += microseconds*1000;

		// Normalize the time to account for the second boundary
		if(deadline.tv_nsec >= 1000000000) {
		    deadline.tv_nsec -= 1000000000;
		    deadline.tv_sec++;
		}
		clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &deadline, NULL);
		//clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &(deadline), NULL);

}

/*
SDL_Surface *gui_load_image(const char *fn)
{

	int imgflags = IMG_INIT_PNG;
	IMG_Init(imgflags);
  SDL_Surface *img = IMG_Load(fn );
	SDL_Surface *tmp = SDL_DisplayFormat(img);

	if(!img)
		return NULL;

	SDL_FreeSurface(img);
	return tmp;
*/

int gui_open()
{
	//screen = scrn;
	//font_tex = gui_load_image("font_1.5x.png");
	SDL_Surface *tmp_font = IMG_Load("font_1.5x.png");

	SDL_SetColorKey(tmp_font,SDL_TRUE,SDL_MapRGB(mappingFormat,255,0,255));
	  //SDL_FillRect(tmp,&tmp->clip_rect,SDL_MapRGB(tmp->format,r,g,b));
		//SDL_BlitSurface(font,NULL,tmp,NULL);
		//SDL_SetColorKey(tmp,SDL_TRUE,SDL_MapRGB(tmp->format,255,0,255));
//SDL_ConvertColorkeyToAlpha

	font_tex =SDL_CreateTextureFromSurface(sdlRenderer,tmp_font) ;//IMG_Load(sdlRenderer,"font_1.5x.png");

	if(!font_tex)
	{
		fprintf(stderr, "Couldn't load font!\n");
		return -1;
	}
	SDL_FreeSurface(tmp_font);
	//SDL_EnableKeyRepeat(250,25);
	return 0;
}


void gui_text(int x, int y, const char *txt, int r, int g ,int b)
{

  //SDL_Surface *tmp = SDL_DisplayFormat(font);
	//SDL_Surface *tmp = SDL_ConvertSurfaceFormat(font_tex,)
//	SDL_Texture *tmp = font_tex;


	//SDL_SetColorKey(font,SDL_TRUE,SDL_MapRGB(font->format,255,255,255));
  //SDL_FillRect(tmp,&tmp->clip_rect,SDL_MapRGB(tmp->format,r,g,b));
	//SDL_BlitSurface(font,NULL,tmp,NULL);
	//SDL_SetColorKey(tmp,SDL_TRUE,SDL_MapRGB(tmp->format,255,0,255));
	//SDL_Rect renderQuad = { 0, 0, 1024, 600 };
	//SDL_RenderCopy( sdlRenderer, font_tex, NULL, &renderQuad );



	//fuck...
	int remap[]={129,90,28,70,89,117,83,84,85,86,28,73,101,102,103,104,56,57,58,59,60,61,62,63,64,65,27,28,28,28,28,0,91,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,83,84,73,0,0,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,115,116};
	//printf("side %lu \n",sizeof(remap)/sizeof(remap[0]));
	int sx = x;
	int sy = y;
	const char *stxt = txt;
	SDL_Rect sr;
	sr.w = FONT_CW;
	sr.h = FONT_CH;
	int fw=408;
	int fh=96;;
	///SDL_QueryTexture(font_tex, NULL, NULL, &fw, &fh);
	SDL_SetTextureColorMod(font_tex, r, g, b	 );

	while(*txt)
	{
		int c = *txt++;

		char a = remap[c-32];

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
			//if(c < ' ' || c > 127)
			//	c = 127;
			c=a;
			sr.x = (c % (fw / FONT_CW)) * FONT_CW;
			sr.y = (c / (fw / FONT_CW)) * FONT_CH;
			dr.x = x;
			dr.y = y;
			dr.w=FONT_CW;
			dr.h=FONT_CW	;

			//printf("sr.x %d sr.y %d \n", sr.x,sr.y);
			SDL_RenderCopy(sdlRenderer, font_tex, &sr, &dr);

			//SDL_BlitSurface(tmp, &sr, dst, &dr);
			//gui_dirty(&dr);
			x += FONT_CW;
			break;

		  }
		}
	}
	//SDL_FreeSurface(tmp);

}


void gui_box(int x, int y, int w, int h, Uint8 rr, Uint8 gg, Uint8 bb)
{

	SDL_SetRenderDrawColor(sdlRenderer, rr,gg,bb,255);


	SDL_Rect r;
	r.x = x;
	r.y = y;
	r.w = w;
	r.h = 1;
	//SDL_FillRect(dst, &r, c);
	SDL_RenderFillRect(sdlRenderer,&r);

	r.x = x;
	r.y = y + h - 1;
	r.w = w;
	r.h = 1;
	//SDL_FillRect(dst, &r, c);
	SDL_RenderFillRect(sdlRenderer,&r);

	r.x = x;
	r.y = y + 1;
	r.w = 1;
	r.h = h - 2;
	//SDL_FillRect(dst, &r, c);
	SDL_RenderFillRect(sdlRenderer,&r);

	r.x = x + w - 1;
	r.y = y + 1;
	r.w = 1;
	r.h = h - 2;
	//SDL_FillRect(dst, &r, c);
	SDL_RenderFillRect(sdlRenderer,&r);

	r.x = x;
	r.y = y;
	r.w = w;
	r.h = h;
//	gui_dirty(&r);
}


void gui_bar(int x, int y, int w, int h, Uint8 rr, Uint8 gg,Uint8 bb)
{
	SDL_Rect r;
	r.x = x;
	r.y = y;
	r.w = w;
	r.h = h;


	//SDL_FillRect(dst, &r, SDL_MapRGB(dst->format, 0, 90, 0));
	SDL_SetRenderDrawColor(sdlRenderer,rr,gg,bb,255);

	SDL_RenderFillRect(sdlRenderer,&r);
	gui_box(x, y, w, h, 0,70,0);
}

void gui_rec()
{

	if(emode){
		gui_text(220, 32, "**REC**", 255,0,199);
	}
	else{
		gui_text(220, 32, "**REC**", 9,255,199);
	}
}


void gui_songpos(int v)
{
	char buf[70];
	snprintf(buf, sizeof(buf), "SongPos: %4.1d CopyPaste: %4.1d   arp: %4.1d  oct:%4.1d", v,cp_len, arp_len,oct);
	gui_text(220+220, 32, buf, 176, 82, 121);
}



void gui_tempo(int v)
{
	char buf[48];
	snprintf(buf, sizeof(buf), "j%2d-Tempo:%4.1d", jump,tempo);
	gui_text(12, 32, buf, 9,255,199);
}

void copy(){
	for(int i =0;i<cp_len;i++){
		cp_buffer[i]=tracks[track][pos+i+16];
		
	}
//	pos+=cp_len;
}

void paste(){

	for(int i=0;i<cp_len;i++){
		tracks[track][pos+i+16]=cp_buffer[i];
		
	}
	pos+=cp_len;	
}


float lerp(float a, float b, float f)
{
    return a + f * (b - a);
}




void blend(){

	int current = tracks[track][pos+16];
	int next =0;
	int stop = 0;
	printf("start blend\n");
	while(next == 0){
		next=tracks[track][pos+16+1+stop];

		if(stop>64){
			break;
		}

		stop++;

	}

	for(int i =1;i<stop;i++){

	//	tracks[track][pos+16+i]=(current*((i)/stop))+(next*(stop-i/stop));
		float t = 1-((float)i/(float)stop);
		//a+t(b-a);//
		//(A*(1024-F) + B * F) >> 10;
		tracks[track][pos+16+i]=(int)lerp(next,current,t);
		//tracks[track][pos+16+i]=current*(66-t) + (next * t);

	}

	pos+=stop;
}

//arp function
//(65,535/20)10.124
///33,177.09375-32,767.5
//
void arp(){
	int current=tracks[track][pos+16];
	for(int i=0;i<arp_len;i++){

		tracks[track][pos+16+i]=current+(410*i);

	}
	pos+=arp_len;
}

void fade_out(int tt,int pp,int ll){
        int current=tracks[tt][pp+16];
        for(int i=0;i<ll;i++){

                //tracks[tt][pp+16+i]=
			
		float result = (float)current* (1- (float)i/(float)ll);
		tracks[tt][pp+16+i]=(int)result;

        }
}



int activity[16];

void gui_songedit(int pos, int ppos, int track, int editing)
{
	int t, n;
	char np[5];
	char note[5];
	char cursor[6];
	int val=0;

	const int y0 = 64;

	/* Track names + cursor */
	gui_text((FONT_CW*4), y0 - FONT_CH,"clk  kik  snr  hat  vox  env  fil  vox  env  fil  mod  mod  mod  T-D  T-E  T-F", 200,255,199);

	/* Notes */


	for(t = 0; t < SSEQ_TRACKS; ++t){

		//indicator color
		if(playing){
				//Uint8 fwc0;
				if(activity[t]>50){
				//fwc0 = SDL_MapRGB(mappingFormat, 255, 0, 255);
				//printf("%d val \n",val/256);
				gui_box((FONT_CW*t*5)+FONT_CW*4, 48 , 48, 16, 255,0,255);
				}
			}
		for(n = 0; n < 32; ++n)
		{

			if(tracks[t][pos+n]){
				snprintf_nowarn(note, sizeof(note), "%04x", tracks[t][pos+n]);
			}
			else{
				note[0]=91;
				note[1]=91;
				note[2]=91;
				note[3]=91;
			}

			int color_r=176;
			int color_g=255;
			int color_b=199;


			if(pos+n-16>-1){
			//n % m == n & (m - 1)
			//bitwise and to replace modulo
			//make text colors
				if((pos+n)&(16-1)){
					if((pos+n)&(4-1)){
						color_r =0;
						color_g =97;
						color_b=77;
						if(tracks[t][pos+n]){

							 color_r=176;
							 color_g=255;
							 color_b=199;
						}
					}
					else{
						color_r=176;
						color_g=82;
						color_b=121;

						if(tracks[t][pos+n]){

							color_r=250;
							color_g=032;
							color_b=071;						}
					}
				}
				else{
					color_r=155;
					color_g=25;
					color_b=99;

					if(tracks[t][pos+n]){
						//255,0,199
						color_r=255;
						color_g=0;
						color_b=199;

					}
			}


				if(t==0){
				snprintf_nowarn(np, sizeof(np), "%.04d", pos+n-16);
				gui_text((0 + FONT_CW * (1 + t*5))-FONT_CW,(y0 + (FONT_CH+8) * (1 + n)) + 3,np, 0,97,77);

				}
				else{
					gui_text(FONT_CW*3 + FONT_CW * (1 + t*5),(y0 + (FONT_CH+8) * (1 + n)) + 3,note, color_r,color_g,color_b);
				}
				memset(note, 0, sizeof note);
				memset(np, 0, sizeof np);
			}
		}
	}
	/* Cursors */


	gui_text((FONT_CW*5 * ( 1+(ppos )))-10, y0, "****", 9,255,199);

	cursor[0]=126;
	cursor[1]=' ';
	cursor[2]=' ';
	cursor[3]=' ';
	cursor[4]=' ';
	cursor[5]=125;


	gui_text((FONT_CW*5 * ( 1+(ppos )))-24,y0 + (FONT_CH+8) * (SSEQ_TRACKS + 1) ,cursor, 255,100,10);
	//Uint8 fwc1 = SDL_MapRGB(mappingFormat, 255, 100, 10);
	//Uint8 fwc2 = SDL_MapRGB(mappingFormat, 255, 110, 100);

	gui_box((FONT_CW*5 * ( 1+(ppos )))-16, (y0 + (FONT_CH+8) * (SSEQ_TRACKS + 1))+2 , 54, 14, 255,100,10);
	gui_box((FONT_CW*5 * ( 1+(ppos )))-16-1, (y0 + (FONT_CH+8) * (SSEQ_TRACKS + 1))+2-1 , 56, 16, 255,110,100);
}




static void draw_main(void)
{
	Uint32 fwc = SDL_MapRGB(mappingFormat, 0, 0, 128);
	SDL_SetRenderDrawColor(sdlRenderer, 0,0,0,255);
		SDL_RenderFillRect(sdlRenderer,NULL);



	/* Clear */
	//SDL_FillRect(screen, NULL, SDL_MapRGB(mappingFormat, 0, 0, 0));

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


void init_dac(){

playing=0;
pos=0;
                sleep_us(10000);

	//disable mux
//	mraa_spi_write(spi,0xB0);
//	mraa_spi_write_word(spi,0x0000);

	//power down all
//	mraa_spi_write(spi,0x50);

	//power up All
//	mraa_spi_write(spi,0x20);

	//config voltage ref
	//mraa_spi_write(spi,0x70);


	//config span
//	mraa_spi_write(spi,0xE0);
//	mraa_spi_write_word(spi,0x0003);

	//zero out dacs
	for(int i=0;i<16;i++){
	mraa_spi_write(spi,0x00);
	mraa_spi_write(spi,0x30+i);
	mraa_spi_write(spi,0x00);
	mraa_spi_write(spi,0x00);
	                sleep_us(10000);
	}
	//set hi
//	for(int i=0;i<16;i++){
//	mraa_spi_write(spi,0x00);
//	mraa_spi_write(spi,0x30+i);
//	mraa_spi_write(spi,0xFF);
//	mraa_spi_write(spi,0xFF);
//	}
	//zero out dacs
//	for(int i=0;i<16;i++){
//	mraa_spi_write(spi,0x00);
//	mraa_spi_write(spi,0x30+i);
//	mraa_spi_write(spi,0x00);
//	mraa_spi_write(spi,0x00);
//	}


}



//handle key control
static void handle_key_ctrl(SDL_Event *ev)
{

	int note_const = 205;

	switch(ev->key.keysym.sym)
	{
          case SDLK_c:
		  copy();
		  break;
	  case SDLK_v:
		  paste();
		  break;
	  case SDLK_h:
		if(cp_len>0){
      		cp_len--;
		}
		break;
          case SDLK_j:
		if(cp_len<128){
		cp_len++;
		}
		break;
				
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
		case SDLK_BACKQUOTE:
			init_dac();
			printf("REINITALIZED DAC\n");
			break;
		case SDLK_HOME:
			pos=0;
			break;
		case SDLK_PAGEUP:
			pos-=8;
			break;
		case SDLK_PAGEDOWN:
			pos+=8;
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
	  case SDLK_m:
	  	emode=!emode;
		break;
	  case SDLK_RETURN:
	  	pos=0;
	  	playing=1;
		break;

	  case SDLK_s:
		break;
	  case SDLK_1:
	  	if(emode){
  			tracks[track][pos+16]=0xFFFF;
				if(!playing){
  			pos+=jump;
  			}
	  	}
		break;
		case SDLK_2:
			if(emode){
				tracks[track][pos+16]+=410;
				if(tracks[track][pos+16]>0xFFFE){
					tracks[track][pos+16]=0xFFFF;
				}
			}
			break;
		case SDLK_3:
	  	if(emode){
  			tracks[track][pos+16]+=205;
				if(tracks[track][pos+16]>0xFFFE){
					tracks[track][pos+16]=0xFFFF;
				}
	  	}
			break;
		case SDLK_4:
			if(emode){
				tracks[track][pos+16]-=410;
				if(tracks[track][pos+16]<0x0001){
					tracks[track][pos+16]=0x0000;
				}
			}
			break;
		case SDLK_5:
			if(emode){
				tracks[track][pos+16]-=205;
				if(tracks[track][pos+16]<0x0001){
					tracks[track][pos+16]=0x0000;
				}
			}
			break;
			case SDLK_6:
			if(emode){
				tracks[track][pos+16]+=102;
				if(tracks[track][pos+16]>0xFFFE){
					tracks[track][pos+16]=0xFFFF;
				}
			}
			break;
			case SDLK_7:
				if(emode){
					tracks[track][pos+16]-=102;
					if(tracks[track][pos+16]<0x0001){
						tracks[track][pos+16]=0x0000;
					}

				}
			break;
			case SDLK_q:
			if(emode){
				tracks[track][pos+16]=32768+(note_const*oct*8);
					if(!playing){
						pos+=jump;
					}
				}
			break;
			case SDLK_w:
			if(emode){
				tracks[track][pos+16]=32768+(note_const)+(note_const*oct*8);
					if(!playing){
						pos+=jump;
					}
				}
			break;
			case SDLK_e:
			if(emode){
				tracks[track][pos+16]=32768+note_const+note_const+(note_const*oct*8);
				//printf("%c \n",tracks[track][pos+16]);
					if(!playing){
						pos+=jump;
					}
				}
			break;
			case SDLK_r:
				if(emode){
					tracks[track][pos+16]=32768+note_const+note_const+note_const+(note_const*oct*8);
						if(!playing){
							pos+=jump;
						}
					}
			break;
			case SDLK_t:
				if(emode){
					tracks[track][pos+16]=32768+note_const+note_const+note_const+note_const+(note_const*oct*8);
						if(!playing){
							pos+=jump;
						}
					}
			break;
			case SDLK_y:
			if(emode){
			tracks[track][pos+16]=32768+note_const+note_const+note_const+note_const+note_const+(note_const*oct*8);
				if(!playing){
					pos+=jump;
				}
			}
			break;
			case SDLK_u:
			if(emode){
			tracks[track][pos+16]=32768+note_const+note_const+note_const+note_const+note_const+note_const+(note_const*oct*8);
				if(!playing){
					pos+=jump;
				}
			}
			break;
			case SDLK_i:
			if(emode){
			tracks[track][pos+16]=32768+(note_const*7)+(note_const*oct*8);
				if(!playing){
					pos+=jump;
				}
			}
			break;
			case SDLK_o:
			if(emode){
			tracks[track][pos+16]=32768+(note_const*8)+(note_const*oct*8);
				if(!playing){
					pos+=jump;
				}
			}
			break;
			case SDLK_p:
			if(emode){
			tracks[track][pos+16]=32768+(note_const*9)+(note_const*oct*8);
				if(!playing){
					pos+=jump;
				}
			}
			break;
		case SDLK_d:
		if(emode){
			tracks[track][pos+16]=0x0000;
			//printf("%c \n",tracks[track][pos+16]);
				if(!playing){
					pos+=jump;
				}
			}
			break;

                case SDLK_z:
                        if(oct<16){
                        oct++;
                        }
                        break;
                case SDLK_x:
                        if(oct>0){
                        oct--;
                        }
			break;



		case SDLK_a:
			if(emode){
				arp();
			}
			break;
		case SDLK_f:
			if(arp_len<128){
			arp_len++;
			}
			break;
		case SDLK_g:
			if(arp_len>0){
			arp_len--;
			}
			break;
		case SDLK_k:
			if(jump>0){
			jump--;
			}

			break;
		case SDLK_l:
			if(jump<96){
			jump++;
			}
			break;
		case SDLK_n:
			f = fopen("tracks.aaa", "wb"); // wb -write binary
			if (f != NULL)
			{
				fwrite(tracks, sizeof(tracks), 1, f);
				printf("wrote file to disk\n");
				fclose(f);
			}
			//free(f);
			break;
		case SDLK_b:
			f = fopen("tracks.aaa", "rb"); // read binary
			if (f != NULL)
			{
				fread(tracks, sizeof(tracks), 1, f);
				printf("read file from disk\n");
				fclose(f);
			}
			//free(f);
			break;
	  case SDLK_0:
			if(tempo<999){
	  	tempo++;
			}
		break;
	  case SDLK_9:
			if(tempo>1){
	  	tempo--;
		}
		break;
	  case SDLK_PERIOD:
		blend();
		break;
	  case SDLK_ESCAPE:
		die=1;
		printf("clean user exit\n \n");
		break;
	  default:
		break;
	}
}

void *kl(void *arg){
while(!die){
		SDL_Event ev;
		//printf("key loop\n");

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
		sleep_us(10000);
	}
}

//handle program close
static void breakhandler(int a)
{
	die = 1;
}

void *step_clock(void *arg)
{
    while(!die)
    {
			//system("clear");

		//	sleep_us((unsigned long)((60000 / tempo/2)/8)*1000);


    	if(playing){
				activity[0]=1-activity[0];
				write_pin(spi,0,0xFFFF);
				write_pin(spi,0,0x7FFF);


				for(int i =1;i<16;i++){
						//65535
						activity[i]=tracks[i][pos+16-1];
						//write_pin(spi,i,0x7FFF);
						if(tracks[i][pos+16-1]==0xFFFF){
							write_pin(spi,i,0xFFFF);
							write_pin(spi,i,0x7FFF);
							//write_pin(spi,i,0x7FFF);
							//break;
						}
						else if(tracks[i][pos+16-1]){
							write_pin(spi,i,tracks[i][pos+16-1]);
							//sleep_us((unsigned long)10000);
							//printf("got value > %d for pin %d\n", tracks[i][pos+16],i);
						}
						//activity[i]/=4;
				}

			//	write_pin(spi,1,0x7FFF);
			//	write_pin(spi,2,0x7FFF);
			//	write_pin(spi,3,0x7FFF);
			//	write_pin(spi,4,0x7FFF);




				pos+=1;
				//tempo clock

    		//60,000 / 100 bpm = 600ms /8 for 32nd notes
    		//long time = ;
    		//printf("%d\n", time);/

	}
			sleep_us((unsigned long)((60000 / tempo)/8)*1000);
	
			//tempo clock

    }
    return 0;
}





/*
void *stop_pin(void *vargs)
{

			struct readThreadParams *params = arg;
			mraa_spi_context *spi = params->spi;
			int pin  = params->pin;

	    pthread_mutex_lock(&spi->mutexBuffer);
			sleep_us(800);
			mraa_spi_write(spi,0x00);
			mraa_spi_write(spi,0x30+pin);
			mraa_spi_write(spi,0x00);
			mraa_spi_write(spi,0x00);
	    pthread_mutex_unlock(&spi->mutexBuffer);
    	return NULL;
*/

float map(float x, float in_min, float in_max, float out_min, float out_max)
{
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}


int midi_to_dac(int note){
	
	int quant = (int)quantizeNote(0,(uint8_t)note);
	
	int out = 0;
	float convert =(float)quant;
	convert*=1.0/12.0;
	convert+=(1.0/12.0)*7;
	convert*=1000.0;
	float max = 65024;
	float half = max/2.0;
	convert = map(convert,0,10666.666,half,max);
	
	out = (int)convert;
	return out;
}

int melody(int offset, int length, float min, float max,int divisor,float line,int vca, int tt,int variance,int layer){

for(int q =0;q<length;q++){


        float rad = 3.141592 / 180;
	float n =0.0;
	for(int i =0;i<layer;i++){
        n += sin(((float)q*line*(i+1))*rad);
	}
	n/=(float)layer;
        int r = (int)map(n,-1,1,min,max);

        if(q%divisor*4==0){
                r-=variance;
        }
        if(q%divisor*2==0){
                r+=variance;
        }

        if(q%divisor==0){
                tracks[tt][q+16]=midi_to_dac(r);
        }

        if(q%divisor==0){
		if(vca>0){
                        tracks[tt+1][q+16]=0xEFFF;
                        fade_out(tt+1,q,vca);

		}
        }


}

}
int modulate(int offset, int length, float min, float max,int divisor,float line,int vca, int tt,int variance){

for(int q =0;q<length;q++){


        float rad = 3.141592 / 180;
        float n = sin(((float)q*line)*rad);
        int r = (int)map(n,-1,1,min,max);

        if(q%divisor*4==0){
                r-=variance;
        }
        if(q%divisor*2==0){
                r+=variance;
        }

        if(q%divisor==0){
                tracks[tt][q+16]=midi_to_dac(r);
        //}i

        //if(q%divisor==0){
                //if(vca>0){
                        //tracks[tt][q+16]=0xEFFF;
                        fade_out(tt,q,vca);

                //}
        }


}

}


int main(int argc, char *argv[])
{

//midi note gen
//21-108
int vca1=0;
int vca2=0;

int filter1=0;
int filter2=0;



//B♭, C, D, E♭, F, G, and A.
//
//
/*
for(int q =0;q<2000;q++){

	time_t t;

	
	float rad = 3.141592 / 180;
   	float n = sin(((float)q*50.0)*rad);
	int r = (int)map(n,-1,1,20,80);

	if(q%32==0){
		r-=4;
	}
	if(q%16==0){
		r+=4;
	}
	
	if(q%4==0){
		tracks[4][q+16]=midi_to_dac(r);
	}

	if(q%4==0){
		//vca1=1-vca1;
		//tracks[5][q+16]=0x0001;
		//if(vca1){
			tracks[5][q+16]=0xEFFF;
			fade_out(5,q,4);

		//}
	}


}
*/

/*
//old melody
melody(0, 3000, 20, 100,4,40.0,4,4,4,24);
//int melody(int offset, int length, float min, float max,int divisor,float line,int vca, int tt,int variance){

melody(0, 3000, 21 ,80,12,10.0,12,7,1,4);

melody(0, 3000, 21, 80,8,24.0,8,10,1,8);

modulate(0, 3000, 20, 90,4,80.0,4,6,2);
modulate(0, 3000, 20, 90,8,120.0,8,9,2);
modulate(0, 3000, 20, 90,16,30.0,16,12,2);

//offset melody
melody(1024, 256, 50, 80,4,80.0,4,4,4,24);
//int melody(int offset, int length, float min, float max,int divisor,float line,int vca, int tt,int variance){

melody(1024, 256, 21 ,80,6,20.0,6,7,4,6);

melody(1024, 256, 21, 80,12,60.0,12,10,4,8);

melody(2048, 256, 50, 80,4,120.0,4,4,4,8);
//int melody(int offset, int length, float min, float max,int divisor,float line,int vca, int tt,int variance){

melody(2048, 256, 21 ,80,6,90.0,6,7,4,8);

melody(2048, 256, 21, 80,12,60.0,12,10,4,16);


*/

/*
for(int q =0;q<2000;q++){

        time_t t;


        float rad = 3.141592 / 180;
        float n = sin(((float)q*200.0)*rad);
        int r = (int)map(n,-1,1,20,108);

        if(q%4==0){
              //  tracks[4][q+16]=midi_to_dac(r);
        }

        if(q%4==0){
                //vca1=1-vca1;
                //tracks[5][q+
		//
		//16]=0x0001;
                //if(vca1){
                        tracks[6][q+16]=midi_to_dac(r);
                        fade_out(6,q,4);

                //}
        }


}*/
 /*
for(int q =0;q<2000;q++){

        time_t t;


        float rad = 3.141592 / 180;
        float n = sin(((float)q*20.0)*rad);
        int r = (int)map(n,-1,1,10,50);

        if(q%32==0){
                r-=4;
        }
        if(q%16==0){
                r+=4;
        }

        if(q%16==0){
                tracks[7][q+16]=midi_to_dac(r);
        }

        if(q%16==0){
                //vca1=1-vca1;
                //tracks[5][q+16]=0x0001;
                //if(vca1){
                        tracks[8][q+16]=0xEFFF;
                        fade_out(8,q,8);

                //}
        }


}


for(int q =0;q<2000;q++){

        time_t t;


        float rad = 3.141592 / 180;
        float n = sin(((float)q*200.0)*rad);
        int r = (int)map(n,-1,1,20,108);

        if(q%4==0){
              //  tracks[4][q+16]=midi_to_dac(r);
        }

        if(q%4==0){
                //vca1=1-vca1;
                //tracks[5][q+16]=0x0001;
                //if(vca1){
                        tracks[9][q+16]=midi_to_dac(r);
                        fade_out(9,q,4);

                //}
        }
}


*/
/*
int arr[32]={1,0,0,0,0,0,0,1,0,1,0,1,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0};
	
	
	//init some kick values to test
	//
	for(int q = 0;q<3000;q++){
		if((q)%8==0){
			//snare
		tracks[2][q+16+4]=0xFFFF;
		}
//kick
int kick = arr[(q%32)];
		if(kick){
			tracks[1][q+16]=0xFFFF;
		}
		if(q%2==0){
			tracks[3][q+16]=0xFFFF;
		}
		if(q%31==0){
			tracks[3][q+16]=0xFFFF;
		}
	}
*/



	
	
	//init some kick values to test
	//

//copy some midi
//
//
//


float x=0.01;
float y=0.0;
float z=0.0;

float dt=.02;
float dx=0.0;
float dy=0.0;
float dz=0.0;

float t,a=10,b=28,c=8/3.0;

int si=0;
	//for(int q = 0;q<3000;q++){
	for(t=0;t<100;t=t+dt){

		dx=a*(y-x)*dt;
		dy=(x*(b-z)-y)*dt;
		dz=(x*y-c*z)*dt;
		x=x+dx;
		y=y+dy;
		z=z+dz;
		printf("xx %f \n",x);
		printf("yy %f \n",y);
		printf("zz %f \n",z);

		if((int)z%4==0){

		//	tracks[1][si+16]=0xFFFF;
		}
		if((int)y%2==0){

		//	tracks[3][si+16]=0xFFFF;
		}
		if((int)x%8==0){
		//	tracks[2][si+16]=0xFFFF;
		}
		si+=1;
		if(si>3000){
			si=0;
		}
	}
	//}


/*
for(int m = 0;m<1565;m++){

	if(m%16==0){
//		tracks[1][m]=0xFFFF;
	}

//	tracks[4][m+16]=song1[m];
//	tracks[7][m+16]=song2[m];
}
*/
	struct sched_param sp;
	sp.sched_priority = 90;

	if(pthread_setschedparam(pthread_self(), SCHED_FIFO, &sp)){
		fprintf(stderr,"WARNING: Failed to set tracker thread"
			"to real-time priority\n");
	}


	//mraa SPI setup
	mraa_result_t status = MRAA_SUCCESS;


  /* initialize mraa for the platform (not needed most of the times) */
  status = mraa_init();
	if(status != MRAA_SUCCESS){
		printf("SPI error \n");
	}

  //! [Interesting]
  /* initialize SPI bus */
  spi = mraa_spi_init(SPI_BUS);


	/* set SPI frequency */
	status = mraa_spi_frequency(spi, SPI_FREQ);
	if(status!= MRAA_SUCCESS){
		printf("SPI error \n");
	}

	/* set big endian mode */
	//lt2668 is MSB LSB
	status = mraa_spi_lsbmode(spi, 1);


	/* set 16 bit mode? not sure if the ltc2668 likes this */
	//ltc 2668 does not like this
	//status = mraa_spi_bit_per_word(spi, 16);
	//mraa_spi_write_word(spi, 0x0f00);


	//txbuf = bytearray(4)
	//txbuf[3] = 0x00
	//txbuf[2] = 0x00
	//#0x30 pin 0 0x31 pin 1 0x32 pin 2 etc etc (in hex) 3B ~ F
	//txbuf[1] = 0x3F
	//txbuf[0] = 0x00
	//power down all 0 1 0 1
	//1 0 0 1 Update All (Power Up)
	/*
	mraa_spi_write(spi,0x00);
	mraa_spi_write(spi,0xFF);
	mraa_spi_write(spi,0x00);
	mraa_spi_write(spi,0xFF);

	mraa_spi_write(spi,0xFF);
	mraa_spi_write(spi,0x00);
	mraa_spi_write(spi,0x00);
	mraa_spi_write(spi,0xFF);
*/

	init_dac();

/*
	//1 1 1 0 Write Span to All
	mraa_spi_write(spi,0xFF);
	mraa_spi_write(spi,0xFF);
	mraa_spi_write(spi,0xFF);
	mraa_spi_write(spi,0x00);


	mraa_spi_write(spi,0x00);
	mraa_spi_write(spi,0x30);
	mraa_spi_write(spi,0xFF);
	mraa_spi_write(spi,0xFF);
*/


	//spawn clock threads
	pthread_t tid;
	pthread_create(&tid, NULL, &step_clock, NULL);


	if(pthread_setschedparam(tid, SCHED_FIFO, &sp)){
		fprintf(stderr,"WARNING: Failed to set tracjer thread"
			"to real-time priority\n");
	}
SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");

	SDL_Init(SDL_INIT_EVERYTHING);
SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
	atexit(SDL_Quit);
	signal(SIGTERM, breakhandler);
	signal(SIGINT, breakhandler);

//SDL_WINDOW_FULLSCREEN | SDL_WINDOW_OPENGL SDL_WINDOW_BORDERLESS
	//SDL_SetRefreshRate(75);
	SDL_ShowCursor(0);
	//SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
	SDL_SetHint(SDL_HINT_RENDER_DRIVER, "opengl");    //New Line

//SDL_WINDOW_FULLSCREEN
	screen = SDL_CreateWindow("tracker",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,1024, 600, SDL_WINDOW_FULLSCREEN | SDL_WINDOW_OPENGL);
	sdlRenderer = SDL_CreateRenderer(screen, -1, SDL_RENDERER_ACCELERATED);
	sdlTexture = SDL_CreateTexture(sdlRenderer,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,1024, 600);
	font_tex = SDL_CreateTexture(sdlRenderer,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STATIC,408,96);
	format = SDL_GetWindowPixelFormat( screen );
  mappingFormat = SDL_AllocFormat( format );

	SDL_RenderSetIntegerScale(sdlRenderer,
                              SDL_TRUE);


	//extern Uint32 *myPixels;  // maybe this is a surface->pixels, or a malloc()'d buffer, or whatever.
	//update buffer

	//SDL_SetRefreshRate(75);
	//2880x1800 ???

	SDL_SetRenderDrawColor(sdlRenderer, 0, 0, 0, 255);
	SDL_RenderClear(sdlRenderer);
	SDL_RenderPresent(sdlRenderer);
	//SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "INTEGER");  // make the scaled rendering look smoother
	//SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_TARGETTEXTURE | SDL_RENDERER_ACCELERATED);


//	SDL_RenderSetLogicalSize(sdlRenderer, 1024, 600);


//	SDL_UpdateTexture(sdlTexture, NULL, myPixels, 1024 * sizeof (Uint32));
	SDL_RenderClear(sdlRenderer);
	SDL_RenderCopy(sdlRenderer, sdlTexture, NULL, NULL);
	SDL_RenderPresent(sdlRenderer);
	 gui_open();

		//spawn clock threads
		pthread_t gfx;
		pthread_create(&gfx, NULL, &kl, NULL);



	while(!die)
	{
		SDL_RenderClear(sdlRenderer);
		if(pos>2998){
			pos=0;
		}


		if(pos<0){
			pos=0;
		}



		draw_main();
		//gui_tempo(tempo);
		//SDL_Flip(screen);

		//SDL_Rect renderQuad = { 0, 0, 1024, 600 };
    //SDL_RenderCopy( sdlRenderer, font_tex, NULL, &renderQuad );

		//SDL_RenderCopy(sdlRenderer, sdlTexture, NULL, NULL);
		SDL_RenderPresent(sdlRenderer);
		//SDL_Delay(10);
	}

	/* stop spi */
	mraa_spi_stop(spi);

	//! [Interesting]
	/* deinitialize mraa for the platform (not needed most of the times) */
	//mraa_deinit();

	SDL_FreeSurface(font);
	font = NULL;
	SDL_Quit();
	return 0;
}
