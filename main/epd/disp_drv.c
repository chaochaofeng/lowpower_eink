#include <stdio.h>
#include <string.h>

#include "disp_drv.h"

struct disp_drv_st *disp_drv_list[] = {
	//&epaper_296x152_drv,
	&epaper_2in66_drv,
};

struct disp_dev_st *disp_dev_list[] = {
	//&epaper_296x152_dev,
	&epaper_2in66_dev,
};

struct disp_dev_st* disp_init(char *name)
{
	struct disp_drv_st *ddrv = NULL;
	struct disp_dev_st *ddev = NULL;

	for (int i=0; i < sizeof(disp_drv_list)/sizeof(disp_drv_list[0]); i++) {
		if (strcmp(name, disp_drv_list[i]->name)) {
			continue;
		}

		ddrv = disp_drv_list[i];
		break;
	}

	if (!ddrv) {
		printf("disp: not support drv:%s\n", name);
		return NULL;
	}

	for (int i=0; i < sizeof(disp_dev_list)/sizeof(disp_dev_list[0]); i++) {
		if (strcmp(name, disp_dev_list[i]->name)) {
			continue;
		}

		ddev = disp_dev_list[i];
		break;
	}

	if (!ddev) {
		printf("disp: not support dev:%s\n", name);
		return NULL;
	}

	ddev->drv = ddrv;

	if (!ddev->drv->init) {
		printf("disp: don't implement init:%s\n", name);
		return ddev;
	}

	ddev->drv->init(ddev);

	return ddev;
}

void disp_flush(struct disp_dev_st *dev,
	unsigned int x, unsigned int y, unsigned int w, unsigned int h, unsigned char *fb)
{
	struct disp_drv_st *ddrv = dev->drv;

	if (!ddrv->flush) {
		printf("disp: don't implement flush:%s\n", ddrv->name);
		return;
	}

	ddrv->flush(dev, x, y, w, h, fb);
}

void disp_clear_screen(struct disp_dev_st *dev)
{
	struct disp_drv_st *ddrv = dev->drv;

	if (!ddrv->clear_screen) {
		printf("disp: don't implement clear_screen:%s\n", ddrv->name);
		return;
	}
	
	ddrv->clear_screen(dev);
}

void disp_clear_area_screen(struct disp_dev_st *dev,
	unsigned int x, unsigned int y, unsigned int w, unsigned int h)
{
	struct disp_drv_st *ddrv = dev->drv;

	if (!ddrv->clear_screen) {
		printf("disp: don't implement clear_screen:%s\n", ddrv->name);
		return;
	}

	ddrv->clear_area_screen(dev, x, y, w, h);
}

void disp_on(struct disp_dev_st *dev)
{
	struct disp_drv_st *ddrv = dev->drv;

	if (!ddrv->disp_on) {
		printf("disp: don't implement disp_on:%s\n", ddrv->name);
		return;
	}
	
	ddrv->disp_on(dev);
}

void disp_off(struct disp_dev_st *dev)
{
	struct disp_drv_st *ddrv = dev->drv;

	if (!ddrv->disp_off) {
		printf("disp: don't implement disp_off:%s\n", ddrv->name);
		return;
	}
	
	ddrv->disp_off(dev);
}


