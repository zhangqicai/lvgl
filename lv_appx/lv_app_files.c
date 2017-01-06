/**
 * @file lv_app_example.c
 *
 */

/*********************
 *      INCLUDES
 *********************/
#include "lv_app_files.h"
#if LV_APP_ENABLE != 0 && USE_LV_APP_FILES != 0

#include <stdio.h>
#include "misc/os/ptask.h"
#include "../lv_app/lv_app_util/lv_app_kb.h"
#include "../lv_app/lv_app_util/lv_app_notice.h"

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/*Application specific data for an instance of this application*/
typedef struct
{
    char path[LV_APP_FILES_PATH_MAX_LEN];
    char fn[LV_APP_FILES_FN_MAX_LEN];
    fs_file_t file;
    uint8_t file_cnt;
    uint16_t chunk_delay;
    uint16_t chunk_size;
    uint8_t send_fn     :1;
    uint8_t send_size   :1;
    uint8_t send_crc    :1;
    uint8_t send_in_prog :1;
    ptask_t * send_task;
}my_app_data_t;

/*Application specific data a window of this application*/
typedef struct
{
    lv_obj_t * file_list;
    lv_obj_t * send_set_h;
}my_win_data_t;

/*Application specific data for a shortcut of this application*/
typedef struct
{
    lv_obj_t * label;
}my_sc_data_t;

typedef enum
{
    SEND_SETTINGS_FN,
    SEND_SETTINGS_SIZE,
    SEND_SETTINGS_CRC,
    SEND_SETTINGS_CHUNK_SIZE,
    SEND_SETTINGS_CHUNK_DELAY,
}send_settings_id_t;

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void my_app_run(lv_app_inst_t * app, void * conf);
static void my_app_close(lv_app_inst_t * app);
static void my_com_rec(lv_app_inst_t * app_send, lv_app_inst_t * app_rec, lv_app_com_type_t type , const void * data, uint32_t size);
static void my_sc_open(lv_app_inst_t * app, lv_obj_t * sc);
static void my_sc_close(lv_app_inst_t * app);
static void my_win_open(lv_app_inst_t * app, lv_obj_t * win);
static void my_win_close(lv_app_inst_t * app);

static void win_load_file_list(lv_app_inst_t * app);
static void win_create_list(lv_app_inst_t * app);
static lv_action_res_t win_up_action(lv_obj_t * up, lv_dispi_t * dispi);
static lv_action_res_t win_next_action(lv_obj_t * next, lv_dispi_t * dispi);
static lv_action_res_t win_prev_action(lv_obj_t * prev, lv_dispi_t * dispi);
static lv_action_res_t win_drv_action(lv_obj_t * drv, lv_dispi_t * dispi);
static lv_action_res_t win_folder_action(lv_obj_t * folder, lv_dispi_t * dispi);
static lv_action_res_t win_file_action(lv_obj_t * file, lv_dispi_t * dispi);
static lv_action_res_t win_send_rel_action(lv_obj_t * send, lv_dispi_t * dispi);
static lv_action_res_t win_send_lpr_action(lv_obj_t * send, lv_dispi_t * dispi);
static lv_action_res_t win_send_settings_element_rel_action(lv_obj_t * element, lv_dispi_t * dispi);
static lv_action_res_t win_back_action(lv_obj_t * back, lv_dispi_t * dispi);
static lv_action_res_t win_del_rel_action(lv_obj_t * send, lv_dispi_t * dispi);
static lv_action_res_t win_del_lpr_action(lv_obj_t * send, lv_dispi_t * dispi);
static void send_settings_kb_close_action(lv_obj_t * ta);
static void send_settings_kb_ok_action(lv_obj_t * ta);
static void start_send(lv_app_inst_t * app, const char * path);
static void send_task(void * param);

/**********************
 *  STATIC VARIABLES
 **********************/
static lv_app_dsc_t my_app_dsc =
{
	.name = "Files",
	.mode = LV_APP_MODE_NONE,
	.app_run = my_app_run,
	.app_close = my_app_close,
	.com_rec = my_com_rec,
	.win_open = my_win_open,
	.win_close = my_win_close,
	.sc_open = my_sc_open,
	.sc_close = my_sc_close,
	.app_data_size = sizeof(my_app_data_t),
	.sc_data_size = sizeof(my_sc_data_t),
	.win_data_size = sizeof(my_win_data_t),
};

static lv_labels_t sc_labels;


/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

/**
 * Initialize the application
 * @return pointer to the application descriptor of this application
 */
const lv_app_dsc_t * lv_app_files_init(void)
{
    lv_app_style_t * app_style = lv_app_style_get();
    memcpy(&sc_labels, &app_style->sc_txt_style, sizeof(lv_labels_t));
    sc_labels.font = LV_APP_FONT_LARGE;


	return &my_app_dsc;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

/**
 * Run an application according to 'app_dsc'
 * @param app_dsc pointer to an application descriptor
 * @param conf pointer to a lv_app_example_conf_t structure with configuration data or NULL if unused
 * @return pointer to the opened application or NULL if any error occurred
 */
static void my_app_run(lv_app_inst_t * app, void * conf)
{
    /*Initialize the application*/
    my_app_data_t * app_data = app->app_data;
    app_data->file_cnt = 0;
    app_data->path[0] = '\0';
    app_data->fn[0] = '\0';
    app_data->send_fn = 0;
    app_data->send_size = 0;
    app_data->send_crc = 0;
    app_data->chunk_size = LV_APP_FILES_CHUNK_DEF_SIZE;
    app_data->chunk_delay = LV_APP_FILES_CHUNK_DEF_TIME;
    app_data->send_in_prog = 0;

    app_data->send_task = ptask_create(send_task, LV_APP_FILES_CHUNK_DEF_TIME, PTASK_PRIO_OFF, app);
}

/**
 * Close a running application.
 * Close the Window and the Shortcut too if opened.
 * Free all the allocated memory by this application.
 * @param app pointer to an application
 */
static void my_app_close(lv_app_inst_t * app)
{
    /*No dynamically allocated data in 'my_app_data'*/
    my_app_data_t * app_data = app->app_data;
    ptask_del(app_data->send_task);
    if(app_data->send_in_prog != 0) fs_close(&app_data->file);

}

/**
 * Read the data have been sent to this application
 * @param app_send pointer to an application which sent the message
 * @param app_rec pointer to an application which is receiving the message
 * @param type type of data from 'lv_app_com_type_t' enum
 * @param data pointer to the sent data
 * @param size length of 'data' in bytes
 */
static void my_com_rec(lv_app_inst_t * app_send, lv_app_inst_t * app_rec,
                       lv_app_com_type_t type , const void * data, uint32_t size)
{
    if(type == LV_APP_COM_TYPE_CHAR) {
        /*Check for file query. E.g. "U:/file.txt?"*/
        const char * path = data;
        if(path[size - 1] == '?') {
            if(size > LV_APP_FILES_PATH_MAX_LEN + LV_APP_FILES_FN_MAX_LEN) {
                lv_app_notice_add("Can not send file:\ntoo long path");
            }

            char path_fn[LV_APP_FILES_PATH_MAX_LEN + LV_APP_FILES_FN_MAX_LEN];
            memcpy(path_fn, data, size - 1); /*-1 to ignore the '?' at the end*/
            path_fn[size - 1] = '\0';
            start_send(app_rec, path_fn);
        }
    }
}

/**
 * Open a shortcut for an application
 * @param app pointer to an application
 * @param sc pointer to an object where the application
 *           can create content of the shortcut
 */
static void my_sc_open(lv_app_inst_t * app, lv_obj_t * sc)
{
    my_sc_data_t * sc_data = app->sc_data;
    my_app_data_t * app_data = app->app_data;


    sc_data->label = lv_label_create(sc, NULL);
    lv_obj_set_style(sc_data->label, &sc_labels);
    lv_label_set_text(sc_data->label, fs_get_last(app_data->path));
    lv_obj_align(sc_data->label, NULL, LV_ALIGN_CENTER, 0, 0);
}

/**
 * Close the shortcut of an application
 * @param app pointer to an application
 */
static void my_sc_close(lv_app_inst_t * app)
{
    /*No dynamically allocated data in 'my_sc_data'*/
}


/**
 * Open the application in a window
 * @param app pointer to an application
 * @param win pointer to a window object where
 *            the application can create content
 */
static void my_win_open(lv_app_inst_t * app, lv_obj_t * win)
{
    my_win_data_t * win_data = app->win_data;
    my_app_data_t * app_data = app->app_data;

    app_data->file_cnt = 0;
    win_data->file_list = NULL;
    win_data->send_set_h = NULL;

    lv_win_set_title(win, app_data->path);

    win_load_file_list(app);
}

/**
 * Close the window of an application
 * @param app pointer to an application
 */
static void my_win_close(lv_app_inst_t * app)
{

}

/*--------------------
 * OTHER FUNCTIONS
 ---------------------*/

static void win_load_file_list(lv_app_inst_t * app)
{
    my_app_data_t * app_data = app->app_data;
    my_win_data_t * win_data = app->win_data;

    /*Create a new list*/
    win_create_list(app);

    fs_res_t res = FS_RES_OK;

      /*At empty path show the drivers */
    lv_obj_t * liste;
    if(app_data->path[0] == '\0') {
        char drv[16];
        char buf[2];
        fs_get_letters(drv);
        uint8_t i;
        for(i = 0; drv[i] != '\0'; i++) {
            buf[0] = drv[i];
            buf[1] = '\0';
            liste = lv_list_add(win_data->file_list, "U:/icon_driver", buf, win_drv_action);
            lv_obj_set_free_p(liste, app);
        }
    }
    /*List the files/folders with fs interface*/
    else {
        liste = lv_list_add(win_data->file_list, "U:/icon_up", "Up", win_up_action);
        lv_obj_set_free_p(liste, app);

        fs_readdir_t rd;
        res = fs_readdir_init(&rd, app_data->path);
        if(res != FS_RES_OK) {
            lv_app_notice_add("Can not read the\npath in Files");
        return;
        }

        /*At not first page add prev. page button */
        if(app_data->file_cnt != 0) {
            liste = lv_list_add(win_data->file_list, "U:/icon_left", "Previous page", win_prev_action);
            lv_obj_set_free_p(liste, app);
        }

        char fn[LV_APP_FILES_FN_MAX_LEN];

        /*Read the files from the previous pages*/
        uint16_t file_cnt = 0;
        while(file_cnt <= app_data->file_cnt) {
            res = fs_readdir(&rd, fn);
            if(res != FS_RES_OK || fn[0] == '\0'){
                lv_app_notice_add("Can not read\nthe path in Files");
                return;
            }
            file_cnt ++;
        }

        /*Add list elements from the files and folders*/
        while(res == FS_RES_OK && fn[0] != '\0') {
            if(fn[0] == '/') { /*Add a folder*/
                lv_obj_t * liste;
                liste = lv_list_add(win_data->file_list, "U:/icon_folder", &fn[1], win_folder_action);
                lv_obj_set_free_p(liste, app);
                app_data->file_cnt ++;
            }
            /*Add a file*/
            else {
                liste = lv_list_add(win_data->file_list, "U:/icon_file", fn, win_file_action);
                lv_obj_set_free_p(liste, app);
                app_data->file_cnt ++;
            }

            /*Get the next element*/
            res = fs_readdir(&rd, fn);

            /*Show only LV_APP_FSEL_MAX_FILE elements and add a Next page button*/
            if(app_data->file_cnt != 0 && app_data->file_cnt % LV_APP_FILES_PAGE_SIZE == 0) {
                liste = lv_list_add(win_data->file_list, "U:/icon_right", "Next page", win_next_action);
                lv_obj_set_free_p(liste, app);
                break;
            }
        }

      /*Close the read directory*/
      fs_readdir_close(&rd);
    }

    if(res != FS_RES_OK) {
        lv_app_notice_add("Can not read\nthe path in Files");
    }

    /*Focus to the top of the list*/
    lv_obj_set_y(lv_page_get_scrl(win_data->file_list), 0);
    return;
}

static void win_create_list(lv_app_inst_t * app)
{
    lv_app_style_t * app_style = lv_app_style_get();
    my_win_data_t * win_data = app->win_data;

    /*Delete the previous list*/
    if(win_data->file_list != NULL) {
      lv_obj_del(win_data->file_list);
    }

    /*Create a new list*/
    win_data->file_list = lv_list_create(app->win, NULL);
    lv_obj_set_width(win_data->file_list, app_style->win_useful_w);
    lv_obj_set_style(win_data->file_list, lv_lists_get(LV_LISTS_TRANSP, NULL));
    lv_list_set_fit(win_data->file_list, LV_LIST_FIT_WIDTH_SB);
    lv_obj_set_drag_parent(win_data->file_list, true);
    lv_obj_set_drag_parent(lv_page_get_scrl(win_data->file_list), true);
    lv_rect_set_fit(win_data->file_list, false, true);
    lv_rect_set_layout(lv_page_get_scrl(win_data->file_list), LV_RECT_LAYOUT_COL_L);
}

/**
 * Called when the Up list element is released to step one level
 * @param up pointer to the Up button
 * @param dispi pointer to the caller display input
 * @return LV_ACTION_RES_INV because the list is deleted in the function
 */
static lv_action_res_t win_up_action(lv_obj_t * up, lv_dispi_t * dispi)
{
    lv_app_inst_t * app = lv_obj_get_free_p(up);
    my_app_data_t * app_data = app->app_data;
    fs_up(app_data->path);
    app_data->file_cnt = 0;
    lv_win_set_title(app->win, app_data->path);

    my_sc_data_t * sc_data = app->sc_data;
    if(sc_data != NULL) {
        lv_label_set_text(sc_data->label, fs_get_last(app_data->path));
        lv_obj_align(sc_data->label, NULL, LV_ALIGN_CENTER, 0, 0);
    }

    win_load_file_list(app);
    return LV_ACTION_RES_INV;
}

/**
 * Called when the Next list element is released to go to the next page
 * @param next pointer to the Next button
 * @param dispi pointer to the caller display input
 * @return LV_ACTION_RES_INV because the list is deleted in the function
 */
static lv_action_res_t win_next_action(lv_obj_t * next, lv_dispi_t * dispi)
{
    lv_app_inst_t * app = lv_obj_get_free_p(next);
    win_load_file_list(app);
    return LV_ACTION_RES_INV;
}

/**
 * Called when the Prev list element is released to previous page
 * @param prev pointer to the Prev button
 * @param dispi pointer to the caller display input
 * @return LV_ACTION_RES_INV because the list is deleted in the function
 */
static lv_action_res_t win_prev_action(lv_obj_t * prev, lv_dispi_t * dispi)
{
    lv_app_inst_t * app = lv_obj_get_free_p(prev);
    my_app_data_t * app_data = app->app_data;
    if(app_data->file_cnt <= 2 * LV_APP_FILES_PAGE_SIZE) app_data->file_cnt = 0;
    else if(app_data->file_cnt % LV_APP_FILES_PAGE_SIZE == 0) {
        app_data->file_cnt -= 2 * LV_APP_FILES_PAGE_SIZE;
    } else {
        app_data->file_cnt = ((app_data->file_cnt / LV_APP_FILES_PAGE_SIZE) - 1) * LV_APP_FILES_PAGE_SIZE;
    }

    win_load_file_list(app);
    return LV_ACTION_RES_INV;
}


/**
 * Called when the Driver list element is released to step into a driver
 * @param drv pointer to the Driver button
 * @param dispi pointer to the caller display input
 * @return LV_ACTION_RES_INV because the list is deleted in the function
 */
static lv_action_res_t win_drv_action(lv_obj_t * drv, lv_dispi_t * dispi)
{
    lv_app_inst_t * app = lv_obj_get_free_p(drv);
    my_app_data_t * app_data = app->app_data;
    sprintf(app_data->path, "%s:", lv_list_element_get_txt(drv));
    app_data->file_cnt = 0;
    lv_win_set_title(app->win, app_data->path);
    my_sc_data_t * sc_data = app->sc_data;
    if(sc_data != NULL) {
        lv_label_set_text(sc_data->label, fs_get_last(app_data->path));
        lv_obj_align(sc_data->label, NULL, LV_ALIGN_CENTER, 0, 0);
    }

    win_load_file_list(app);
    return LV_ACTION_RES_INV;
}


/**
 * Called when a folder list element is released to enter into it
 * @param folder pointer to a folder button
 * @param dispi pointer to the caller display input
 * @return LV_ACTION_RES_INV because the list is deleted in the function
 */
static lv_action_res_t win_folder_action(lv_obj_t * folder, lv_dispi_t * dispi)
{
    lv_app_inst_t * app = lv_obj_get_free_p(folder);
    my_app_data_t * app_data = app->app_data;
    sprintf(app_data->path, "%s/%s", app_data->path, lv_list_element_get_txt(folder));
    app_data->file_cnt = 0;

    lv_win_set_title(app->win, app_data->path);
    my_sc_data_t * sc_data = app->sc_data;
    if(sc_data != NULL) {
        lv_label_set_text(sc_data->label, fs_get_last(app_data->path));
        lv_obj_align(sc_data->label, NULL, LV_ALIGN_CENTER, 0, 0);
    }


    win_load_file_list(app);
    return LV_ACTION_RES_INV;
}


/**
 * Called when a file list element is released to choose it
 * @param file pointer to a file button
 * @param dispi pointer to the caller display input
 * @return LV_ACTION_RES_INV because the list is deleted in the function
 */
static lv_action_res_t win_file_action(lv_obj_t * file, lv_dispi_t * dispi)
{
    lv_app_inst_t * app = lv_obj_get_free_p(file);
    my_app_data_t * app_data = app->app_data;
    my_win_data_t * win_data = app->win_data;

    sprintf(app_data->fn, "%s", lv_list_element_get_txt(file));

    win_create_list(app);

    lv_obj_t * liste;
    liste = lv_list_add(win_data->file_list, "U:/icon_left", "Back", win_back_action);
    lv_obj_set_free_p(liste, app);

    liste = lv_list_add(win_data->file_list, NULL, "Send", win_send_rel_action);
    lv_obj_set_free_p(liste, app);
    lv_btn_set_lpr_action(liste, win_send_lpr_action);
    lv_obj_set_free_p(liste, app);

    liste = lv_list_add(win_data->file_list, NULL, "Delete", win_del_rel_action);
    lv_btn_set_lpr_action(liste, win_del_lpr_action);
    lv_obj_set_free_p(liste, app);

    return LV_ACTION_RES_INV;
}

static lv_action_res_t win_send_rel_action(lv_obj_t * send, lv_dispi_t * dispi)
{
    lv_app_inst_t * app = lv_obj_get_free_p(send);
    my_app_data_t * app_data = app->app_data;

    if(app_data->send_in_prog != 0) {
        lv_app_notice_add("File sending\nin progress");
        return LV_ACTION_RES_OK;
    }


    char path_fn[LV_APP_FILES_PATH_MAX_LEN + LV_APP_FILES_FN_MAX_LEN];
    sprintf(path_fn, "%s/%s", app_data->path, app_data->fn);
    start_send(app, path_fn);



    return LV_ACTION_RES_OK;

}

static lv_action_res_t win_send_lpr_action(lv_obj_t * send, lv_dispi_t * dispi)
{
    lv_app_inst_t * app = lv_obj_get_free_p(send);
    my_app_data_t * app_data = app->app_data;
    my_win_data_t * win_data = app->win_data;

    if(win_data->send_set_h != NULL) {
        lv_obj_del(win_data->send_set_h);
        win_data->send_set_h = NULL;
        lv_dispi_wait_release(dispi);
        lv_btn_set_state(send, LV_BTN_STATE_REL);
        return LV_ACTION_RES_OK;
    }

    lv_btn_set_state(send, LV_BTN_STATE_REL);
    lv_rect_set_layout(send, LV_RECT_LAYOUT_COL_L);

    win_data->send_set_h = lv_rect_create(send, NULL);
    lv_obj_set_style(win_data->send_set_h, lv_rects_get(LV_RECTS_TRANSP, NULL));
    lv_obj_set_click(win_data->send_set_h, false);
    lv_rect_set_fit(win_data->send_set_h, true, true);
    lv_rect_set_layout(win_data->send_set_h, LV_RECT_LAYOUT_COL_L);

    lv_obj_t * cb;

    cb = lv_cb_create(win_data->send_set_h, NULL);
    lv_cb_set_text(cb, "Send file name");
    lv_obj_set_free_num(cb, SEND_SETTINGS_FN);
    lv_obj_set_free_p(cb, app);
    lv_btn_set_rel_action(cb, win_send_settings_element_rel_action);
    if(app_data->send_fn != 0) lv_btn_set_state(cb, LV_BTN_STATE_TGL_REL);
    else lv_btn_set_state(cb, LV_BTN_STATE_REL);

    cb = lv_cb_create(win_data->send_set_h, cb);
    lv_cb_set_text(cb, "Send size");
    lv_obj_set_free_num(cb, SEND_SETTINGS_SIZE);
    if(app_data->send_size != 0) lv_btn_set_state(cb, LV_BTN_STATE_TGL_REL);
    else lv_btn_set_state(cb, LV_BTN_STATE_REL);

    cb = lv_cb_create(win_data->send_set_h, cb);
    lv_cb_set_text(cb, "Send CRC");
    lv_obj_set_free_num(cb, SEND_SETTINGS_CRC);
    if(app_data->send_crc != 0) lv_btn_set_state(cb, LV_BTN_STATE_TGL_REL);
    else lv_btn_set_state(cb, LV_BTN_STATE_REL);

    lv_obj_t * val_set_h;

    val_set_h = lv_rect_create(win_data->send_set_h, NULL);
    lv_obj_set_style(val_set_h, lv_rects_get(LV_RECTS_TRANSP, NULL));
    lv_obj_set_click(val_set_h, false);
    lv_rect_set_fit(val_set_h, true, true);
    lv_rect_set_layout(val_set_h, LV_RECT_LAYOUT_ROW_M);

    lv_obj_t * label;
    label = lv_label_create(val_set_h, NULL);
    lv_label_set_text(label, "Chunk size");

    lv_obj_t * ta;
    char buf[32];
    ta = lv_ta_create(val_set_h, NULL);
    lv_obj_set_style(ta, lv_tas_get(LV_TAS_SIMPLE, NULL));
    lv_rect_set_fit(ta, false, true);
    lv_obj_set_free_num(ta, SEND_SETTINGS_CHUNK_SIZE);
    lv_obj_set_free_p(ta, app);
    lv_page_set_rel_action(ta, win_send_settings_element_rel_action);
    sprintf(buf, "%d", app_data->chunk_size);
    lv_ta_set_text(ta, buf);

    val_set_h = lv_rect_create(win_data->send_set_h, val_set_h);

    label = lv_label_create(val_set_h, NULL);
    lv_label_set_text(label, "Inter-chunk delay");

    ta = lv_ta_create(val_set_h, ta);
    lv_obj_set_free_num(ta, SEND_SETTINGS_CHUNK_DELAY);
    sprintf(buf, "%d", app_data->chunk_delay);
    lv_ta_set_text(ta, buf);

    return LV_ACTION_RES_INV;
}

static lv_action_res_t win_send_settings_element_rel_action(lv_obj_t * element, lv_dispi_t * dispi)
{
    send_settings_id_t id = lv_obj_get_free_num(element);
    lv_app_inst_t * app = lv_obj_get_free_p(element);
    my_app_data_t * app_data = app->app_data;

    if(id == SEND_SETTINGS_FN) {
        app_data->send_fn = lv_btn_get_state(element) == LV_BTN_STATE_REL ? 0 : 1;
    } else if(id == SEND_SETTINGS_SIZE) {
        app_data->send_size = lv_btn_get_state(element) == LV_BTN_STATE_REL ? 0 : 1;
    } else if(id == SEND_SETTINGS_CRC) {
        app_data->send_crc = lv_btn_get_state(element) == LV_BTN_STATE_REL ? 0 : 1;
    } else if(id == SEND_SETTINGS_CHUNK_SIZE) {
        lv_app_kb_open(element, LV_APP_KB_MODE_NUM, send_settings_kb_close_action, send_settings_kb_ok_action);
    } else if(id == SEND_SETTINGS_CHUNK_DELAY) {
        lv_app_kb_open(element, LV_APP_KB_MODE_NUM, send_settings_kb_close_action, send_settings_kb_ok_action);
    }

    return LV_ACTION_RES_OK;
}
/**
 * Called when the Back list element is released to when a file chosen to go back to the file list
 * @param back pointer to the back button
 * @param dispi pointer to the caller display input
 * @return LV_ACTION_RES_INV because the list is deleted in the function
 */
static lv_action_res_t win_back_action(lv_obj_t * up, lv_dispi_t * dispi)
{
    lv_app_inst_t * app = lv_obj_get_free_p(up);
    my_app_data_t * app_data = app->app_data;

    app_data->file_cnt = 0;
    win_load_file_list(app);
    return LV_ACTION_RES_INV;
}


static lv_action_res_t win_del_rel_action(lv_obj_t * send, lv_dispi_t * dispi)
{
    lv_app_notice_add("Press long the Delete button\n"
                      "to remove the file");

    return LV_ACTION_RES_OK;
}

static lv_action_res_t win_del_lpr_action(lv_obj_t * send, lv_dispi_t * dispi)
{
    lv_app_inst_t * app = lv_obj_get_free_p(send);
    my_app_data_t * app_data = app->app_data;

    char path_fn[LV_APP_FILES_PATH_MAX_LEN + LV_APP_FILES_FN_MAX_LEN];
    sprintf(path_fn, "%s/%s", app_data->path, app_data->fn);

    fs_res_t res = fs_remove(path_fn);
    if(res == FS_RES_OK) lv_app_notice_add("%s deleted", app_data->fn);
    else lv_app_notice_add("Can not delete\n%s", app_data->fn);

    return LV_ACTION_RES_OK;
}
static void send_settings_kb_close_action(lv_obj_t * ta)
{
    send_settings_id_t id = lv_obj_get_free_num(ta);
    lv_app_inst_t * app = lv_obj_get_free_p(ta);
    my_app_data_t * app_data = app->app_data;

    char buf[32];
    buf[0] = '\0';

    if(id == SEND_SETTINGS_CHUNK_DELAY) {
        sprintf(buf, "%d", app_data->chunk_size);
    } else if(id == SEND_SETTINGS_CHUNK_SIZE) {
        sprintf(buf, "%d", app_data->chunk_size);
    }
    lv_ta_set_text(ta, buf);
}

static void send_settings_kb_ok_action(lv_obj_t * ta)
{
    send_settings_id_t id = lv_obj_get_free_num(ta);
    lv_app_inst_t * app = lv_obj_get_free_p(ta);
    my_app_data_t * app_data = app->app_data;

    int num;
    sscanf(lv_ta_get_txt(ta), "%d", &num);

    if(id == SEND_SETTINGS_CHUNK_DELAY) {
        app_data->chunk_delay = (uint16_t) num;
    } else if(id == SEND_SETTINGS_CHUNK_SIZE) {
        if(num > LV_APP_FILES_CHUNK_MAX_SIZE) num = LV_APP_FILES_CHUNK_MAX_SIZE;
        app_data->chunk_size= (uint16_t) num;
    }

}

static void start_send(lv_app_inst_t * app, const char * path)
{
    my_app_data_t * app_data = app->app_data;
    fs_res_t res = fs_open(&app_data->file, path, FS_MODE_RD);
    if(res == FS_RES_OK) {
        uint32_t rn;
        char rd_buf[LV_APP_FILES_CHUNK_MAX_SIZE];
        res = fs_read(&app_data->file, rd_buf, app_data->chunk_size, &rn);
        if(res == FS_RES_OK) {
            app_data->send_in_prog = 1;
            lv_app_com_send(app, LV_APP_COM_TYPE_CHAR, rd_buf, rn);

            ptask_set_period(app_data->send_task, app_data->chunk_delay);
            ptask_reset(app_data->send_task);
            ptask_set_prio(app_data->send_task, PTASK_PRIO_HIGH);
        }
    }

   if(res != FS_RES_OK) {
        fs_close(&app_data->file);
        app_data->send_in_prog = 0;
        lv_app_notice_add("Can not send\nthe file in Files");
   } else {
       lv_app_notice_add("Sending\n%s", fs_get_last(path));

       if(app->sc_data != NULL) {
           my_sc_data_t * sc_data = app->sc_data;

           uint32_t size;
           fs_size(&app_data->file, &size);
           uint32_t pos;
           fs_tell(&app_data->file, &pos);

           int pct = (uint32_t) (pos * 100) / size;

           char buf[256];
           sprintf(buf, "Sending\n%d%%", pct);
           lv_label_set_text(sc_data->label, buf);
           lv_obj_align(sc_data->label, NULL, LV_ALIGN_CENTER, 0, 0);
       }
   }

}

static void send_task(void * param)
{
    lv_app_inst_t * app = param;
    my_app_data_t * app_data = app->app_data;

    if(app_data->send_in_prog == 0) return;

    uint32_t rn;
    char rd_buf[LV_APP_FILES_CHUNK_MAX_SIZE];
    fs_res_t res = fs_read(&app_data->file, rd_buf, app_data->chunk_size, &rn);
    if(res == FS_RES_OK) {
       app_data->send_in_prog = 1;
       lv_app_com_send(app, LV_APP_COM_TYPE_CHAR, rd_buf, rn);
    }

    if(res != FS_RES_OK) {
       fs_close(&app_data->file);
       app_data->send_in_prog = 0;
       lv_app_notice_add("Can not send\nthe file in Files");
    } else {
        my_sc_data_t * sc_data = app->sc_data;

        if(rn < app_data->chunk_size) {
            lv_app_notice_add("File sent");
            fs_close(&app_data->file);
            app_data->send_in_prog = 0;

            if(sc_data != NULL) {
                lv_label_set_text(sc_data->label, fs_get_last(app_data->path));
                lv_obj_align(sc_data->label, NULL, LV_ALIGN_CENTER, 0, 0);
            }
        } else  {
            if(sc_data != NULL) {
                uint32_t size;
                fs_size(&app_data->file, &size);
                uint32_t pos;
                fs_tell(&app_data->file, &pos);

                uint8_t pct = (uint32_t) (pos * 100) / size;

                char buf[256];
                sprintf(buf, "Sending\n%d%%", pct);
                lv_label_set_text(sc_data->label, buf);
                lv_obj_align(sc_data->label, NULL, LV_ALIGN_CENTER, 0, 0);
            }
        }
    }
}

#endif /*LV_APP_ENABLE != 0 && USE_LV_APP_EXAMPLE != 0*/
