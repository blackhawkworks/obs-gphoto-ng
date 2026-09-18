#include <obs/obs-module.h>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("obs-gphoto", "en-US")

MODULE_EXPORT const char *obs_module_name(void)
{
	return "obs-gphoto";
}

MODULE_EXPORT const char *obs_module_description(void)
{
	return "Allows connecting DSLR cameras with OBS Studio through gPhoto on Linux";
}

MODULE_EXPORT const char *obs_module_author(void)
{
	return "obs-gphoto contributors";
}

extern struct obs_source_info capture_preview_info;
extern struct obs_source_info timelapse_capture_info;

bool obs_module_load(void)
{
	obs_register_source(&capture_preview_info);
	obs_register_source(&timelapse_capture_info);
	return true;
}