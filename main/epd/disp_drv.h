#ifndef _DISP_DRV_H
#define _DISP_DRV_H

#define EPD_BLACK	0
#define EPD_RED		1

#define EPD_BACK	0
#define EPD_FRONT	1

struct disp_dev_st;

struct disp_drv_st
{
	char name[32];
	int (*init)(struct disp_dev_st *);
	void (*uninit)(struct disp_dev_st *);
	void (*flush)(struct disp_dev_st *,
		unsigned int x, unsigned int y, unsigned int w, unsigned int h, unsigned char *fb);
	void (*draw_pixel)(struct disp_dev_st *, unsigned int x, unsigned int y, unsigned char color);
	void (*clear_screen)(struct disp_dev_st *);
	void (*clear_area_screen)(struct disp_dev_st *,
		unsigned int x, unsigned int y, unsigned int w, unsigned int h);
	void (*disp_on)(struct disp_dev_st *);
	void (*disp_off)(struct disp_dev_st *);
	void *priv_data[2];
};

struct disp_dev_st
{
	unsigned int width;
	unsigned int height;
	char name[32];
	unsigned char draw_color;
	struct disp_drv_st *drv;
	void *buff[2];
	unsigned char flash_all;
	unsigned char buf_index;
};

extern struct disp_drv_st epaper_104x212_drv;
extern struct disp_dev_st epaper_104x212_dev;

#define EPD_296x152 1
#if EPD_296x152
extern struct disp_drv_st epaper_296x152_drv;
extern struct disp_dev_st epaper_296x152_dev;
#endif

extern struct disp_drv_st epaper_2in66_drv;
extern struct disp_dev_st epaper_2in66_dev;

struct disp_dev_st* disp_init(char *name);
void disp_flush(struct disp_dev_st *dev,
	unsigned int x, unsigned int y, unsigned int w, unsigned int h, unsigned char *fb);
void disp_clear_screen(struct disp_dev_st *dev);
void disp_clear_area_screen(struct disp_dev_st *dev,
	unsigned int x, unsigned int y, unsigned int w, unsigned int h);
void disp_on(struct disp_dev_st *dev);
void disp_off(struct disp_dev_st *dev);

#endif
