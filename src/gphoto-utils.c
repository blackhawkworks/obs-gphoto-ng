#include <obs/obs-module.h>
#include <gphoto2/gphoto2-camera.h>
#include <MagickCore/MagickCore.h>
#include <string.h>
#include <pthread.h>

#include "gphoto-preview.h"

static GPPortInfoList       *portinfolist = NULL;
static CameraAbilitiesList *abilities = NULL;

static int sample_open_camera(Camera **camera, const char *model, const char *port, GPContext *context) {
    int ret, m, p;
    CameraAbilities a;
    GPPortInfo pi;

    ret = gp_camera_new(camera);
    if (ret < GP_OK) return ret;

    if (!abilities) {
        ret = gp_abilities_list_new(&abilities);
        if (ret < GP_OK) return ret;
        ret = gp_abilities_list_load(abilities, context);
        if (ret < GP_OK) return ret;
    }

    m = gp_abilities_list_lookup_model(abilities, model);
    if (m < GP_OK) return m;
    ret = gp_abilities_list_get_abilities(abilities, m, &a);
    if (ret < GP_OK) return ret;
    ret = gp_camera_set_abilities(*camera, a);
    if (ret < GP_OK) return ret;

    if (!portinfolist) {
        ret = gp_port_info_list_new(&portinfolist);
        if (ret < GP_OK) return ret;
        ret = gp_port_info_list_load(portinfolist);
        if (ret < 0) return ret;
        ret = gp_port_info_list_count(portinfolist);
        if (ret < 0) return ret;
    }

    p = gp_port_info_list_lookup_path(portinfolist, port);
    if (p < GP_OK) return p;

    ret = gp_port_info_list_get_info(portinfolist, p, &pi);
    if (ret < GP_OK) return ret;
    ret = gp_camera_set_port_info(*camera, pi);
    if (ret < GP_OK) return ret;
    return GP_OK;
}

int gp_camera_by_name(Camera **camera, const char *name, CameraList *cam_list, GPContext *context) {
    int i, count, ret;
    const char *camera_name, *usb_port;

    count = gp_list_count(cam_list);
    for (i = 0; i < count; i++) {
        gp_list_get_name(cam_list, i, &camera_name);
        gp_list_get_value(cam_list, i, &usb_port);
        if (camera_name && name && strcmp(camera_name, name) == 0) {
            ret = sample_open_camera(camera, camera_name, usb_port, context);
            return ret;
        }
    }
    return GP_ERROR;
}

void property_cam_list(CameraList *cam_list, obs_property_t *prop) {
    int i, count;
    const char *camera_name;

    obs_property_list_clear(prop);

    count = gp_list_count(cam_list);
    blog(LOG_INFO, "Number of cameras: %d\n", count);
    for (i = 0; i < count; i++) {
        gp_list_get_name(cam_list, i, &camera_name);
        if (camera_name) {
            obs_property_list_add_string(prop, camera_name, camera_name);
        }
    }
}

void gphoto_capture_preview(Camera *camera, GPContext *context, int width, int height, uint8_t *texture_data) {
    CameraFile *cam_file = NULL;
    const char *image_data = NULL;
    size_t data_size = 0;
    Image *image = NULL;
    ImageInfo *image_info = AcquireImageInfo();
    ExceptionInfo *exception = AcquireExceptionInfo();

    if (gp_file_new(&cam_file) < GP_OK) {
        blog(LOG_WARNING, "Failed to create gphoto CameraFile handle.\n");
    } else {
        if (gp_camera_capture_preview(camera, cam_file, context) < GP_OK) {
            blog(LOG_DEBUG, "Can't capture preview.\n");
        } else {
            if (gp_file_get_data_and_size(cam_file, &image_data, &data_size) < GP_OK) {
                blog(LOG_WARNING, "Can't get image data.\n");
            } else {
                image = BlobToImage(image_info, image_data, data_size, exception);
                if (exception->severity != UndefinedException) {
                    CatchException(exception);
                    blog(LOG_WARNING, "ImageMagick error: %s.\n", exception->reason ? exception->reason : "Unknown");
                    exception->severity = UndefinedException;
                } else if (image) {
                    ExportImagePixels(image, 0, 0, (size_t)width, (size_t)height, "BGRA", CharPixel, texture_data, exception);
                    if (exception->severity != UndefinedException) {
                        CatchException(exception);
                        blog(LOG_WARNING, "ImageMagick export error: %s.\n", exception->reason ? exception->reason : "Unknown");
                        exception->severity = UndefinedException;
                    }
                }
            }
        }
    }

    if (image_info) {
        DestroyImageInfo(image_info);
    }
    if (image) {
        DestroyImageList(image);
    }
    if (exception) {
        DestroyExceptionInfo(exception);
    }
    if (cam_file) {
        gp_file_unref(cam_file);
    }
}

void gphoto_capture(Camera *camera, GPContext *context, int width, int height, uint8_t *texture_data) {
    CameraFile *cam_file = NULL;
    CameraFilePath camera_file_path;
    const char *image_data = NULL;
    size_t data_size = 0;
    Image *image = NULL;
    ImageInfo *image_info = AcquireImageInfo();
    ExceptionInfo *exception = AcquireExceptionInfo();

    if (gp_file_new(&cam_file) < GP_OK) {
        blog(LOG_WARNING, "Failed to create gphoto CameraFile handle.\n");
    } else {
        if (gp_camera_capture(camera, GP_CAPTURE_IMAGE, &camera_file_path, context) < GP_OK) {
            blog(LOG_WARNING, "Can't capture photo.\n");
        } else {
            if (gp_camera_file_get(camera, camera_file_path.folder, camera_file_path.name,
                                   GP_FILE_TYPE_NORMAL, cam_file, context) < GP_OK) {
                blog(LOG_WARNING, "Can't get photo from camera.\n");
            } else {
                if (gp_file_get_data_and_size(cam_file, &image_data, &data_size) < GP_OK) {
                    blog(LOG_WARNING, "Can't get image data.\n");
                } else {
                    gp_camera_file_delete(camera, camera_file_path.folder, camera_file_path.name, context);
                    image = BlobToImage(image_info, image_data, data_size, exception);
                    if (exception->severity != UndefinedException) {
                        CatchException(exception);
                        blog(LOG_WARNING, "ImageMagick error: %s.\n", exception->reason ? exception->reason : "Unknown");
                        exception->severity = UndefinedException;
                    } else if (image) {
                        ExportImagePixels(image, 0, 0, (size_t)width, (size_t)height, "BGRA", CharPixel, texture_data, exception);
                        if (exception->severity != UndefinedException) {
                            CatchException(exception);
                            blog(LOG_WARNING, "ImageMagick export error: %s.\n", exception->reason ? exception->reason : "Unknown");
                            exception->severity = UndefinedException;
                        }
                    }
                }
            }
        }
    }

    if (image_info) {
        DestroyImageInfo(image_info);
    }
    if (image) {
        DestroyImageList(image);
    }
    if (exception) {
        DestroyExceptionInfo(exception);
    }
    if (cam_file) {
        gp_file_unref(cam_file);
    }
}

int gphoto_cam_list(CameraList *cam_list, GPContext *context) {
    gp_list_reset(cam_list);
    return gp_camera_autodetect(cam_list, context);
}

int cancel_autofocus(Camera *camera, GPContext *context) {
    int ret = -1;
    CameraWidget *widget = NULL;
    int toggle;

    if (gp_camera_get_single_config(camera, "autofocusdrive", &widget, context) == GP_OK) {
        toggle = 0;
        gp_widget_set_value(widget, &toggle);
        ret = gp_camera_set_single_config(camera, "autofocusdrive", widget, context);
    }

    if (gp_camera_get_single_config(camera, "cancelautofocus", &widget, context) == GP_OK) {
        toggle = 1;
        gp_widget_set_value(widget, &toggle);
        gp_camera_set_single_config(camera, "cancelautofocus", widget, context);
    }

    if (widget) {
        gp_widget_free(widget);
    }
    return ret;
}

obs_property_t *camera_config_to_obs_property(char *config_name, Camera *camera, GPContext *context, obs_properties_t *props,
                                              const char *prop_name, const char *prop_description) {
    CameraWidget *widget = NULL;
    CameraWidgetType type;
    obs_property_t *p = NULL;
    int i, count;
    float min, max, step;
    const char *choose_val;
    if (gp_camera_get_single_config(camera, config_name, &widget, context) < GP_OK) {
        blog(LOG_WARNING, "Can't get single config %s for camera.\n", config_name);
    } else {
        if (gp_widget_get_type(widget, &type) < GP_OK) {
            blog(LOG_WARNING, "Can't get type for config %s.\n", config_name);
        } else {
            switch (type) {
                case GP_WIDGET_TEXT:
                    p = obs_properties_add_text(props, prop_name, obs_module_text(prop_description),
                                                OBS_TEXT_DEFAULT);
                    goto out;
                case GP_WIDGET_RANGE:
                    if (gp_widget_get_range(widget, &min, &max, &step) == GP_OK) {
                        p = obs_properties_add_float_slider(props, prop_name, obs_module_text(prop_description),
                                                            min, max, step);
                    }
                    goto out;
                case GP_WIDGET_TOGGLE:
                    p = obs_properties_add_bool(props, prop_name, obs_module_text(prop_description));
                    goto out;
                case GP_WIDGET_RADIO:
                case GP_WIDGET_MENU:
                    p = obs_properties_add_list(props, prop_name, obs_module_text(prop_description),
                                                OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
                    count = gp_widget_count_choices(widget);
                    for (i = 0; i < count; i++) {
                        if (gp_widget_get_choice(widget, i, &choose_val) == GP_OK) {
                            obs_property_list_add_string(p, choose_val, choose_val);
                        }
                    }
                    goto out;
                default:
                    goto out;
            }
        }
    }
out:
    if (widget) {
        gp_widget_free(widget);
    }
    return p;
}

static bool obs_property_callback(obs_properties_t *props, obs_property_t *prop, obs_data_t *settings) {
    UNUSED_PARAMETER(props);
    obs_data_set_string(settings, "changed", "auto_prop");
    obs_data_set_string(settings, "auto_prop", obs_property_name(prop));
    return true;
}

int create_obs_property_from_camera_config(obs_properties_t *props, obs_data_t *settings, const char *prop_description,
                                           Camera *camera, GPContext *context, char *config_name) {
    int ret = -1;
    char *text;
    float range;
    int toggle;
    char *radio;
    obs_property_t *p;
    CameraWidget *widget = NULL;
    p = camera_config_to_obs_property(config_name, camera, context, props, config_name, prop_description);
    if (!p) return ret;
    obs_property_set_modified_callback(p, obs_property_callback);
    enum obs_property_type p_type = obs_property_get_type(p);
    switch (p_type) {
        case OBS_PROPERTY_TEXT:
            ret = gp_camera_get_single_config(camera, config_name, &widget, context);
            if (ret == GP_OK) {
                gp_widget_get_value(widget, &text);
                obs_data_set_default_string(settings, config_name, text);
            }
            goto out;
        case OBS_PROPERTY_FLOAT:
            ret = gp_camera_get_single_config(camera, config_name, &widget, context);
            if (ret == GP_OK) {
                gp_widget_get_value(widget, &range);
                obs_data_set_default_double(settings, config_name, (double)range);
            }
            goto out;
        case OBS_PROPERTY_BOOL:
            ret = gp_camera_get_single_config(camera, config_name, &widget, context);
            if (ret == GP_OK) {
                gp_widget_get_value(widget, &toggle);
                obs_data_set_default_bool(settings, config_name, toggle != 0);
            }
            goto out;
        case OBS_PROPERTY_LIST:
            ret = gp_camera_get_single_config(camera, config_name, &widget, context);
            if (ret == GP_OK) {
                gp_widget_get_value(widget, &radio);
                obs_data_set_default_string(settings, config_name, radio);
            }
            goto out;
        default:
            goto out;
    }
out:
    if (widget) {
        gp_widget_free(widget);
    }
    return ret;
}

int set_camera_config(obs_data_t *settings, Camera *camera, GPContext *context) {
    int ret = -1;
    const char *text;
    float range;
    int toggle;
    CameraWidget *widget = NULL;
    CameraWidgetType type;
    const char *name = obs_data_get_string(settings, "auto_prop");

    if (!name || strlen(name) == 0) return ret;

    cancel_autofocus(camera, context);

    if (gp_camera_get_single_config(camera, name, &widget, context) < GP_OK) {
        blog(LOG_WARNING, "Can't get single config %s for camera.\n", name);
    } else {
        if (gp_widget_get_type(widget, &type) < GP_OK) {
            blog(LOG_WARNING, "Can't get type for config %s.\n", name);
        } else {
            switch (type) {
                case GP_WIDGET_TEXT:
                    text = obs_data_get_string(settings, name);
                    ret = gp_widget_set_value(widget, text);
                    goto out;
                case GP_WIDGET_RANGE:
                    range = (float)obs_data_get_double(settings, name);
                    ret = gp_widget_set_value(widget, &range);
                    goto out;
                case GP_WIDGET_TOGGLE:
                    toggle = obs_data_get_bool(settings, name) ? 1 : 0;
                    ret = gp_widget_set_value(widget, &toggle);
                    goto out;
                case GP_WIDGET_RADIO:
                case GP_WIDGET_MENU:
                    text = obs_data_get_string(settings, name);
                    ret = gp_widget_set_value(widget, text);
                    goto out;
                default:
                    goto out;
            }
        }
    }
out:
    if (widget) {
        ret = gp_camera_set_single_config(camera, name, widget, context);
        gp_widget_free(widget);
    }
    return ret;
}

static bool autofocus_property_callback(obs_properties_t *props, obs_property_t *prop, obs_data_t *settings) {
    UNUSED_PARAMETER(prop);
    obs_data_set_string(settings, "changed", "autofocus");
    bool enabled = obs_data_get_bool(settings, "autofocusdrive");

    obs_property_t *manualfocusdrive = obs_properties_get(props, "manualfocusdrive");
    obs_property_t *near1 = obs_properties_get(props, "Near 1");
    obs_property_t *near2 = obs_properties_get(props, "Near 2");
    obs_property_t *near3 = obs_properties_get(props, "Near 3");
    obs_property_t *far1 = obs_properties_get(props, "Far 1");
    obs_property_t *far2 = obs_properties_get(props, "Far 2");
    obs_property_t *far3 = obs_properties_get(props, "Far 3");

    if (manualfocusdrive) obs_property_set_visible(manualfocusdrive, !enabled);
    if (near1) obs_property_set_visible(near1, !enabled);
    if (near2) obs_property_set_visible(near2, !enabled);
    if (near3) obs_property_set_visible(near3, !enabled);
    if (far1) obs_property_set_visible(far1, !enabled);
    if (far2) obs_property_set_visible(far2, !enabled);
    if (far3) obs_property_set_visible(far3, !enabled);

    return true;
}

int create_autofocus_property(obs_properties_t *props, obs_data_t *settings, Camera *camera, GPContext *context) {
    int ret = -1;
    int toggle = 0;
    CameraWidget *widget = NULL;
    CameraWidgetType type;
    obs_property_t *p;

    if (gp_camera_get_single_config(camera, "autofocusdrive", &widget, context) < GP_OK) {
        blog(LOG_WARNING, "Can't get single config autofocusdrive for camera.\n");
    } else {
        if (gp_widget_get_type(widget, &type) < GP_OK) {
            blog(LOG_WARNING, "Can't get type for config autofocusdrive.\n");
        } else {
            if (type == GP_WIDGET_TOGGLE) {
                p = obs_properties_add_bool(props, "autofocusdrive", obs_module_text("Auto Focus"));
                obs_property_set_modified_callback(p, autofocus_property_callback);
                gp_widget_get_value(widget, &toggle);
                obs_data_set_default_bool(settings, "autofocusdrive", false);
            }
        }
    }
    if (widget) {
        gp_widget_free(widget);
    }
    return ret;
}

int set_autofocus(Camera *camera, GPContext *context) {
    int ret = -1;
    CameraWidget *widget = NULL;
    CameraWidgetType type;
    int toggle = 1;

    cancel_autofocus(camera, context);

    if (gp_camera_get_single_config(camera, "autofocusdrive", &widget, context) < GP_OK) {
        blog(LOG_WARNING, "Can't get single config autofocusdrive for camera.\n");
    } else {
        if (gp_widget_get_type(widget, &type) < GP_OK) {
            blog(LOG_WARNING, "Can't get type for config autofocusdrive.\n");
        } else {
            if (type == GP_WIDGET_TOGGLE) {
                gp_widget_set_value(widget, &toggle);
                ret = gp_camera_set_single_config(camera, "autofocusdrive", widget, context);
            }
        }
    }
    if (widget) {
        gp_widget_free(widget);
    }
    return ret;
}

int set_manualfocus(const char *value, Camera *camera, GPContext *context) {
    int ret = -1;
    CameraWidget *widget = NULL;
    CameraWidgetType type;

    cancel_autofocus(camera, context);

    if (gp_camera_get_single_config(camera, "manualfocusdrive", &widget, context) < GP_OK) {
        blog(LOG_WARNING, "Can't get single config manualfocusdrive for camera.\n");
    } else {
        if (gp_widget_get_type(widget, &type) < GP_OK) {
            blog(LOG_WARNING, "Can't get type for config manualfocusdrive.\n");
        } else {
            if (type == GP_WIDGET_RADIO) {
                ret = gp_widget_set_value(widget, value);
            }
        }
    }
    if (widget) {
        ret = gp_camera_set_single_config(camera, "manualfocusdrive", widget, context);
        gp_widget_free(widget);
    }
    return ret;
}

static bool manual_radio_focus_callback(obs_properties_t *props, obs_property_t *prop, void *vptr) {
    UNUSED_PARAMETER(props);
    struct preview_data *data = vptr;
    if (!data || !prop) return true;
    const char *value = obs_property_name(prop);
    pthread_mutex_lock(&data->camera_mutex);
    set_manualfocus(value, data->camera, data->gp_context);
    pthread_mutex_unlock(&data->camera_mutex);

    return true;
}

int create_manualfocus_property(obs_properties_t *props, obs_data_t *settings, Camera *camera, GPContext *context) {
    int ret = -1, count;
    float min, max, step, range;
    CameraWidget *widget = NULL;
    CameraWidgetType type;
    obs_property_t *p;
    if (gp_camera_get_single_config(camera, "manualfocusdrive", &widget, context) == GP_OK) {
        if (gp_widget_get_type(widget, &type) == GP_OK) {
            if (type == GP_WIDGET_RADIO) {
                count = gp_widget_count_choices(widget);
                if (count == 7) {
                    obs_properties_add_button(props, "Near 3", "<<<", manual_radio_focus_callback);
                    obs_properties_add_button(props, "Near 2", "<<", manual_radio_focus_callback);
                    obs_properties_add_button(props, "Near 1", "<", manual_radio_focus_callback);
                    obs_properties_add_button(props, "Far 1", ">", manual_radio_focus_callback);
                    obs_properties_add_button(props, "Far 2", ">>", manual_radio_focus_callback);
                    obs_properties_add_button(props, "Far 3", ">>>", manual_radio_focus_callback);
                }
            } else if (type == GP_WIDGET_RANGE) {
                if (gp_widget_get_range(widget, &min, &max, &step) == GP_OK) {
                    p = obs_properties_add_float_slider(props, "manualfocusdrive", obs_module_text("Manual focus"),
                                                        min, max, step);
                    obs_property_set_modified_callback(p, obs_property_callback);
                    gp_widget_get_value(widget, &range);
                    obs_data_set_default_double(settings, "manualfocusdrive", (double)range);
                }
            }
        }
    }
    if (widget) {
        gp_widget_free(widget);
    }
    return ret;
}