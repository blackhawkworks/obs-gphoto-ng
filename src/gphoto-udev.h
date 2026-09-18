#ifndef GPHOTO_UDEV_H
#define GPHOTO_UDEV_H

#include <libudev.h>
#include <obs/obs-module.h>
#include <obs/callback/signal.h>

#ifdef __cplusplus
extern "C" {
#endif

void gphoto_init_udev(void);
void gphoto_unref_udev(void);
signal_handler_t *gphoto_get_udev_signalhandler(void);

#ifdef __cplusplus
}
#endif

#endif /* GPHOTO_UDEV_H */