#pragma bank 255

#include <gbdk/platform.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "musicmanager.h"
#include "joy.h"
#include "gbcamera.h"
#include "screen.h"
#include "states.h"
#include "gbprinter.h"
#include "linkcable.h"
#include "remote.h"
#include "vector.h"
#include "load_save.h"
#include "fade_manager.h"
#include "protected.h"
#include "vwf.h"

#include "state_camera.h"
#include "state_gallery.h"

#include "misc_assets.h"

// audio assets
#include "sound_ok.h"
#include "sound_error.h"
#include "sound_transmit.h"
#include "sound_menu_alter.h"

// menus
#include "menus.h"
#include "menu_codes.h"
#include "menu_main.h"
#include "menu_yesno.h"
#include "menu_debug.h"
#include "menu_settings.h"

// frames
#include "print_frames.h"

#if (DEBUG_ENABLED==1)
    #define MENUITEM_DEBUG_NEXT GalleryMenuItemDebug
    #define MENUITEM_FIRST_PREV GalleryMenuItemDebug
    #define MENUITEM_LAST_FLAGS 0
    #define GALLERYMENU_HEIGHT 11
#else
    #define MENUITEM_DEBUG_NEXT GalleryMenuItemInfo
    #define MENUITEM_FIRST_PREV GalleryMenuItemUndeleteAll
    #define MENUITEM_LAST_FLAGS MENUITEM_TERM
    #define GALLERYMENU_HEIGHT 10
#endif


BANKREF(state_gallery)

#define VECTOR_SET_DIRECT(VECTOR, POS, ELEM) ((VECTOR[(POS) + 1]) = (ELEM))

VECTOR_DECLARE(used_slots, uint8_t, CAMERA_MAX_IMAGE_SLOTS);
VECTOR_DECLARE(free_slots, uint8_t, CAMERA_MAX_IMAGE_SLOTS);

void gallery_toss_images() {
    SWITCH_RAM(CAMERA_BANK_LAST_SEEN);
    memset(used_slots, CAMERA_IMAGE_DELETED, sizeof(used_slots));
    VECTOR_CLEAR(free_slots);
    uint8_t elem;
    for (uint8_t i = 0; i != CAMERA_MAX_IMAGE_SLOTS; i++) {
        uint8_t order = cam_game_data.imageslots[i];
        if (order < CAMERA_MAX_IMAGE_SLOTS) {
            elem = VECTOR_GET(used_slots, order);
            if (elem == CAMERA_IMAGE_DELETED) VECTOR_SET_DIRECT(used_slots, order, i); else VECTOR_ADD(free_slots, i);
        } else VECTOR_ADD(free_slots, i);
    }
    uint8_t j = 0;
    for (uint8_t i = 0; i != CAMERA_MAX_IMAGE_SLOTS; i++) {
        elem = VECTOR_GET(used_slots, i);
        if (elem < CAMERA_MAX_IMAGE_SLOTS) VECTOR_SET_DIRECT(used_slots, j++, elem);
    }
    VECTOR_LEN(used_slots) = j;
    protected_pack(used_slots);
    VECTOR_ITERATE(free_slots, j, elem) protected_modify_slot(elem, CAMERA_IMAGE_DELETED);
}

uint8_t gallery_show_picture(uint8_t image_no) {
    wait_vbl_done();
    screen_clear_rect(IMAGE_DISPLAY_X, IMAGE_DISPLAY_Y, CAMERA_IMAGE_TILE_WIDTH, CAMERA_IMAGE_TILE_HEIGHT, SOLID_BLACK);

    if (!images_taken()) return CAMERA_IMAGE_DELETED;

    uint8_t displayed_index = (image_no < images_taken()) ? image_no : (images_taken() - 1);

    uint8_t image_index = VECTOR_GET(used_slots, displayed_index);
    SWITCH_RAM((image_index >> 1) + 1);
    screen_load_image(IMAGE_DISPLAY_X, IMAGE_DISPLAY_Y, CAMERA_IMAGE_TILE_WIDTH, CAMERA_IMAGE_TILE_HEIGHT, ((image_index & 1) ? image_second : image_first));

    wait_vbl_done();
    screen_restore_rect(IMAGE_DISPLAY_X, IMAGE_DISPLAY_Y, CAMERA_IMAGE_TILE_WIDTH, CAMERA_IMAGE_TILE_HEIGHT);

    return displayed_index;
}

uint8_t gallery_show_position(uint8_t image_no) {
    sprintf(text_buffer, "%hd/%hd", (uint8_t)((image_no < images_taken()) ? (image_no + 1) : 0), (uint8_t)images_taken());
    menu_text_out(HELP_CONTEXT_WIDTH, 17, IMAGE_SLOTS_USED_WIDTH, SOLID_BLACK, text_buffer);
    return image_no;
}

uint8_t gallery_print_picture(uint8_t image_no, uint8_t frame_no) BANKED {
    if (image_no < images_taken()) {
        uint8_t image_index = VECTOR_GET(used_slots, image_no);
        if (gbprinter_detect(10) == PRN_STATUS_OK) {
            return (gbprinter_print_image(((image_index & 1) ? image_second : image_first),
                                          (image_index >> 1) + 1,
                                          print_frames + frame_no,
                                          BANK(print_frames)) == PRN_STATUS_CANCELLED) ? FALSE : TRUE;
        }
    }
    return FALSE;
}

uint8_t gallery_transfer_picture(uint8_t image_no) BANKED {
    if (image_no < images_taken()) {
        uint8_t image_index = VECTOR_GET(used_slots, image_no);
        linkcable_transfer_image(((image_index & 1) ? image_second : image_first), (image_index >> 1) + 1);
        return TRUE;
    }
    return FALSE;
}

static uint8_t onPrinterProgress() BANKED {
    // printer progress callback handler
    gallery_show_progressbar(0, printer_completion, PRN_MAX_PROGRESS);
    return 0;
}

uint8_t onShowImageInfo(const struct menu_t * self, uint8_t * param);
uint8_t onTranslateKeyImageInfo(const struct menu_t * menu, const struct menu_item_t * self, uint8_t value);
const menu_item_t ImageInfoMenuItemOk = {
    .prev = &ImageInfoMenuItemPrint,    .next = &ImageInfoMenuItemPrint,
    .sub = NULL, .sub_params = NULL,
    .ofs_x = 12, .ofs_y = 14, .width = 0,
    .caption = " " ICON_A " OK ",
    .onPaint = NULL,
    .result = MENU_RESULT_CLOSE
};
const menu_item_t ImageInfoMenuItemPrint = {
    .prev = &ImageInfoMenuItemOk,       .next = &ImageInfoMenuItemOk,
    .sub = NULL, .sub_params = NULL,
    .ofs_x = 7, .ofs_y = 14, .width = 0, .flags = MENUITEM_TERM,
    .caption = " Print...",
    .onPaint = NULL,
    .result = ACTION_PRINT_INFO
};
const menu_t ImageInfoMenu = {
    .x = 3, .y = 1, .width = 17, .height = 16,
    .items = &ImageInfoMenuItemOk,
    .onShow = onShowImageInfo, .onTranslateKey = onTranslateKeyImageInfo, .onTranslateSubResult = NULL
};
uint8_t onShowImageInfo(const menu_t * self, uint8_t * param) {
    static image_metadata_t image_metadata;
    param;
    if (OPTION(gallery_picture_idx) < images_taken()) {
        uint8_t slot = VECTOR_GET(used_slots, OPTION(gallery_picture_idx));
        image_metadata.settings = current_settings[OPTION(camera_mode)];
        protected_metadata_read(slot, (uint8_t *)&image_metadata, sizeof(image_metadata));
        if (image_metadata.crc == protected_calculate_crc((uint8_t *)&image_metadata.settings, sizeof(image_metadata.settings), PROTECTED_SEED)) {
            vwf_set_tab_size(1);
            menu_text_out(self->x + 4, self->y + 1,  0, SOLID_WHITE, "Image data:");
            menu_text_out(self->x + 1, self->y + 2,  0, SOLID_WHITE, camera_format_item_text(idExposure,      strcpy(text_buffer_extra_ex, "Exposure\t%sms"),   &image_metadata.settings));
            menu_text_out(self->x + 1, self->y + 3,  0, SOLID_WHITE, camera_format_item_text(idGain,          strcpy(text_buffer_extra_ex, "Gain\t\t\t\t%s"),   &image_metadata.settings));
            menu_text_out(self->x + 1, self->y + 4,  0, SOLID_WHITE, camera_format_item_text(idContrast,      strcpy(text_buffer_extra_ex, "Contrast\t\t%d"),   &image_metadata.settings));
            menu_text_out(self->x + 1, self->y + 5,  0, SOLID_WHITE, camera_format_item_text(idDither,        strcpy(text_buffer_extra_ex, "Dithering\t\t%s"),  &image_metadata.settings));
            menu_text_out(self->x + 1, self->y + 6,  0, SOLID_WHITE, camera_format_item_text(idDitherLight,   strcpy(text_buffer_extra_ex, "Dith. light\t%s"),  &image_metadata.settings));
            menu_text_out(self->x + 1, self->y + 7,  0, SOLID_WHITE, camera_format_item_text(idInvOutput,     strcpy(text_buffer_extra_ex, "Inverse\t\t\t%s"),  &image_metadata.settings));
            menu_text_out(self->x + 1, self->y + 8,  0, SOLID_WHITE, camera_format_item_text(idZeroPoint,     strcpy(text_buffer_extra_ex, "Zero point\t%s"),   &image_metadata.settings));
            menu_text_out(self->x + 1, self->y + 9,  0, SOLID_WHITE, camera_format_item_text(idVOut,          strcpy(text_buffer_extra_ex, "Volt. out.\t%dmv"), &image_metadata.settings));
            menu_text_out(self->x + 1, self->y + 10, 0, SOLID_WHITE, camera_format_item_text(idVoltageRef,    strcpy(text_buffer_extra_ex, "Volt. ref.\t%sv"),  &image_metadata.settings));
            menu_text_out(self->x + 1, self->y + 11, 0, SOLID_WHITE, camera_format_item_text(idEdgeOperation, strcpy(text_buffer_extra_ex, "Edge op.\t\t%s"),   &image_metadata.settings));
            menu_text_out(self->x + 1, self->y + 12, 0, SOLID_WHITE, camera_format_item_text(idEdgeRatio,     strcpy(text_buffer_extra_ex, "Edge ratio\t%s"),   &image_metadata.settings));
            menu_text_out(self->x + 1, self->y + 13, 0, SOLID_WHITE, camera_format_item_text(idEdgeExclusive, strcpy(text_buffer_extra_ex, "Edge excl.\t%s"),   &image_metadata.settings));
            vwf_set_tab_size(2);
        } else menu_text_out(self->x + 4, self->y + 1, 0, SOLID_WHITE, "No data...");
        SWITCH_RAM((slot >> 1) + 1);
        screen_load_thumbnail(self->x + 12, self->y + 2, ((slot & 1) ? image_second_thumbnail : image_first_thumbnail), 0x00);
        screen_restore_rect(self->x + 12, self->y + 2, CAMERA_THUMB_TILE_WIDTH, CAMERA_THUMB_TILE_HEIGHT);
    } else menu_text_out(self->x + 4, self->y + 1, 0, SOLID_WHITE, "No image...");
    return 0;
}
uint8_t onTranslateKeyImageInfo(const struct menu_t * menu, const struct menu_item_t * self, uint8_t value) {
    menu; self;
    if (value & J_LEFT) value |= J_UP;
    if (value & J_RIGHT) value |= J_DOWN;
    return value;
}

uint8_t onHelpGalleryMenu(const struct menu_t * menu, const struct menu_item_t * selection);
uint8_t onTranslateSubResultGalleryMenu(const struct menu_t * menu, const struct menu_item_t * self, uint8_t value);
const menu_item_t GalleryMenuItemInfo = {
    .prev = &MENUITEM_FIRST_PREV,           .next = &GalleryMenuItemPrint,
    .sub = &ImageInfoMenu, .sub_params = NULL,
    .ofs_x = 1, .ofs_y = 1, .width = 8,
    .caption = " Info",
    .helpcontext = " View image metadata",
    .onPaint = NULL,
    .result = MENU_RESULT_CLOSE
};
const menu_item_t GalleryMenuItemPrint = {
    .prev = &GalleryMenuItemInfo,           .next = &GalleryMenuItemPrintAll,
    .sub = NULL, .sub_params = NULL,
    .ofs_x = 1, .ofs_y = 2, .width = 8,
    .caption = " Print",
    .helpcontext = " Print current image",
    .onPaint = NULL,
    .result = ACTION_PRINT_IMAGE
};
const menu_item_t GalleryMenuItemPrintAll = {
    .prev = &GalleryMenuItemPrint,          .next = &GalleryMenuItemTransfer,
    .sub = NULL, .sub_params = NULL,
    .ofs_x = 1, .ofs_y = 3, .width = 8,
    .caption = " Print all",
    .helpcontext = " Print the whole gallery",
    .onPaint = NULL,
    .result = ACTION_PRINT_GALLERY
};
const menu_item_t GalleryMenuItemTransfer = {
    .prev = &GalleryMenuItemPrintAll,       .next = &GalleryMenuItemTransferAll,
    .sub = NULL, .sub_params = NULL,
    .ofs_x = 1, .ofs_y = 4, .width = 8,
    .caption = " Transfer",
    .helpcontext = " Transfer using link cable",
    .onPaint = NULL,
    .result = ACTION_TRANSFER_IMAGE
};
const menu_item_t GalleryMenuItemTransferAll = {
    .prev = &GalleryMenuItemTransfer,       .next = &GalleryMenuItemDelete,
    .sub = NULL, .sub_params = NULL,
    .ofs_x = 1, .ofs_y = 5, .width = 8,
    .caption = " Transfer all",
    .helpcontext = " Transfer the whole gallery",
    .onPaint = NULL,
    .result = ACTION_TRANSFER_GALLERY
};
const menu_item_t GalleryMenuItemDelete = {
    .prev = &GalleryMenuItemTransferAll,    .next = &GalleryMenuItemDeleteAll,
    .sub = &YesNoMenu, .sub_params = "Are you sure?",
    .ofs_x = 1, .ofs_y = 6, .width = 8,
    .caption = " Delete",
    .helpcontext = " Delete current image",
    .onPaint = NULL,
    .result = ACTION_ERASE_IMAGE
};
const menu_item_t GalleryMenuItemDeleteAll = {
    .prev = &GalleryMenuItemDelete,         .next = &GalleryMenuItemUndeleteAll,
    .sub = &YesNoMenu, .sub_params = "Delete all images?",
    .ofs_x = 1, .ofs_y = 7, .width = 8,
    .caption = " Delete all",
    .helpcontext = " Erase the whole gallery",
    .onPaint = NULL,
    .result = ACTION_ERASE_GALLERY
};
const menu_item_t GalleryMenuItemUndeleteAll = {
    .prev = &GalleryMenuItemDeleteAll,      .next = &MENUITEM_DEBUG_NEXT,
    .sub = &YesNoMenu, .sub_params = "Undelete all images?",
    .ofs_x = 1, .ofs_y = 8, .width = 8, .flags = MENUITEM_LAST_FLAGS,
    .caption = " Undelete all",
    .helpcontext = " Unerase the whole gallery",
    .onPaint = NULL,
    .result = ACTION_UNERASE_GALLERY
};
#if (DEBUG_ENABLED==1)
const menu_item_t GalleryMenuItemDebug = {
    .prev = &GalleryMenuItemUndeleteAll,    .next = &GalleryMenuItemInfo,
    .sub = &DebugMenu, .sub_params = NULL,
    .ofs_x = 1, .ofs_y = 9, .width = 8, .flags = MENUITEM_TERM,
    .caption = " Debug",
    .helpcontext = " Show debug info",
    .onPaint = NULL,
    .result = MENU_RESULT_CLOSE
};
#endif
const menu_t GalleryMenu = {
    .x = 1, .y = 3, .width = 10, .height = GALLERYMENU_HEIGHT,
    .cancel_mask = J_B, .cancel_result = ACTION_NONE,
    .items = &GalleryMenuItemInfo,
    .onShow = NULL, .onHelpContext = onHelpGalleryMenu,
    .onTranslateKey = NULL, .onTranslateSubResult = onTranslateSubResultGalleryMenu
};

uint8_t onTranslateSubResultGalleryMenu(const struct menu_t * menu, const struct menu_item_t * self, uint8_t value) {
    menu;
    if (self->sub == &YesNoMenu) {
        return (value == MENU_RESULT_YES) ? self->result : ACTION_NONE;
    }
    return value;
}
uint8_t onHelpGalleryMenu(const struct menu_t * menu, const struct menu_item_t * selection) {
    menu;
    // we draw help context here
    menu_text_out(0, 17, HELP_CONTEXT_WIDTH, SOLID_BLACK, selection->helpcontext);
    return 0;
}


uint8_t gallery_print_info() {
    if (gbprinter_detect(10) == PRN_STATUS_OK) {
        return (gbprinter_print_screen_rect(ImageInfoMenu.x + 1, ImageInfoMenu.y + 1, ImageInfoMenu.width - 2, ImageInfoMenu.height - 3, TRUE) == PRN_STATUS_CANCELLED) ? FALSE : TRUE;
    }
    return FALSE;
}

static uint8_t refresh_screen() {
    screen_clear_rect(0, 0, 20, 18, SOLID_BLACK);
    menu_text_out(0, 0, 20, SOLID_BLACK, " Gallery view");
    menu_text_out(0, 17, HELP_CONTEXT_WIDTH, SOLID_BLACK, " " ICON_START " or " ICON_SELECT "/" ICON_B " for Menus");
    return gallery_show_position(gallery_show_picture(OPTION(gallery_picture_idx)));
}

uint8_t INIT_state_gallery() BANKED {
    gallery_toss_images();
    return 0;
}

uint8_t ENTER_state_gallery() BANKED {
    OPTION(gallery_picture_idx) = refresh_screen();
    gbprinter_set_handler(onPrinterProgress, BANK(state_gallery));
    fade_in_modal();
    JOYPAD_RESET();
    return 0;
}

uint8_t UPDATE_state_gallery() BANKED {
    static uint8_t menu_result;
    PROCESS_INPUT();
    if (KEY_PRESSED(J_UP) || KEY_PRESSED(J_RIGHT)) {
        // next image
        if (images_taken() > 1) {
            music_play_sfx(BANK(sound_menu_alter), sound_menu_alter, SFX_MUTE_MASK(sound_menu_alter), MUSIC_SFX_PRIORITY_MINIMAL);
            if (++OPTION(gallery_picture_idx) == images_taken()) OPTION(gallery_picture_idx) = 0;
            gallery_show_position(gallery_show_picture(OPTION(gallery_picture_idx)));
            save_camera_state();
        }
    } else if (KEY_PRESSED(J_DOWN) || KEY_PRESSED(J_LEFT)) {
        // previous image
        if (images_taken() > 1) {
            music_play_sfx(BANK(sound_menu_alter), sound_menu_alter, SFX_MUTE_MASK(sound_menu_alter), MUSIC_SFX_PRIORITY_MINIMAL);
            if (OPTION(gallery_picture_idx)) --OPTION(gallery_picture_idx); else OPTION(gallery_picture_idx) = images_taken() - 1;
            gallery_show_position(gallery_show_picture(OPTION(gallery_picture_idx)));
            save_camera_state();
        }
    } else if (KEY_PRESSED(J_A)) {
        // switch to thumbnail view
        if (images_taken()) {
            music_play_sfx(BANK(sound_ok), sound_ok, SFX_MUTE_MASK(sound_ok), MUSIC_SFX_PRIORITY_MINIMAL);
            CHANGE_STATE(state_thumbnails);
            return FALSE;
        }
    } else if ((KEY_PRESSED(J_SELECT)) || (KEY_PRESSED(J_B))) {
        switch (menu_result = menu_execute(&GalleryMenu, NULL, NULL)) {
            case ACTION_ERASE_GALLERY:
                if (images_taken() != 0) {
                    VECTOR_CLEAR(used_slots), VECTOR_CLEAR(free_slots);
                    for (uint8_t i = CAMERA_MAX_IMAGE_SLOTS; i != 0; i--) {
                        protected_modify_slot(i - 1, CAMERA_IMAGE_DELETED);
                        VECTOR_ADD(free_slots, i - 1);
                    }
                    music_play_sfx(BANK(sound_ok), sound_ok, SFX_MUTE_MASK(sound_ok), MUSIC_SFX_PRIORITY_MINIMAL);
                } else music_play_sfx(BANK(sound_error), sound_error, SFX_MUTE_MASK(sound_error), MUSIC_SFX_PRIORITY_MINIMAL);
                break;
            case ACTION_ERASE_IMAGE:
                if ((images_taken()) && (OPTION(gallery_picture_idx) < images_taken())) {
                    uint8_t elem = VECTOR_GET(used_slots, OPTION(gallery_picture_idx));
                    VECTOR_DEL(used_slots, OPTION(gallery_picture_idx));
                    protected_modify_slot(elem, CAMERA_IMAGE_DELETED);
                    VECTOR_ADD(free_slots, elem);
                    protected_pack(used_slots);
                    music_play_sfx(BANK(sound_ok), sound_ok, SFX_MUTE_MASK(sound_ok), MUSIC_SFX_PRIORITY_MINIMAL);
                } else music_play_sfx(BANK(sound_error), sound_error, SFX_MUTE_MASK(sound_error), MUSIC_SFX_PRIORITY_MINIMAL);
                break;
            case ACTION_UNERASE_GALLERY:
                while (VECTOR_LEN(free_slots)) {
                    uint8_t elem = VECTOR_POP(free_slots);
                    protected_modify_slot(elem, images_taken());
                    VECTOR_ADD(used_slots, elem);
                }
                break;
            case ACTION_TRANSFER_IMAGE:
                music_play_sfx(BANK(sound_transmit), sound_transmit, SFX_MUTE_MASK(sound_transmit), MUSIC_SFX_PRIORITY_MINIMAL);
                remote_activate(REMOTE_DISABLED);
                linkcable_transfer_reset();
                if (!gallery_transfer_picture(OPTION(gallery_picture_idx))) {
                    music_play_sfx(BANK(sound_error), sound_error, SFX_MUTE_MASK(sound_error), MUSIC_SFX_PRIORITY_MINIMAL);
                }
                remote_activate(REMOTE_ENABLED);
                JOYPAD_RESET();
                break;
            case ACTION_PRINT_IMAGE:
                remote_activate(REMOTE_DISABLED);
                if (!gallery_print_picture(OPTION(gallery_picture_idx), OPTION(print_frame_idx))) {
                    music_play_sfx(BANK(sound_error), sound_error, SFX_MUTE_MASK(sound_error), MUSIC_SFX_PRIORITY_MINIMAL);
                }
                remote_activate(REMOTE_ENABLED);
                JOYPAD_RESET();
                break;
            case ACTION_TRANSFER_GALLERY:
            case ACTION_PRINT_GALLERY:
                gbprinter_set_handler(NULL, 0);
                remote_activate(REMOTE_DISABLED);
                if (menu_result == ACTION_TRANSFER_GALLERY) {
                    linkcable_transfer_reset();
                    music_play_sfx(BANK(sound_transmit), sound_transmit, SFX_MUTE_MASK(sound_transmit), MUSIC_SFX_PRIORITY_MINIMAL);
                }
                uint8_t transfer_completion = 0, image_count = images_taken();
                gallery_show_progressbar(0, 0, PRN_MAX_PROGRESS);
                for (uint8_t i = 0; i != images_taken(); i++) {
                    if (!((menu_result == ACTION_TRANSFER_GALLERY) ? gallery_transfer_picture(i) : gallery_print_picture(i, OPTION(print_frame_idx)))) {
                        music_play_sfx(BANK(sound_error), sound_error, SFX_MUTE_MASK(sound_error), MUSIC_SFX_PRIORITY_MINIMAL);
                        break;
                    }
                    uint8_t current_progress = (((uint16_t)(i + 1) * PRN_MAX_PROGRESS) / image_count);
                    if (transfer_completion != current_progress) {
                        transfer_completion = current_progress;
                        gallery_show_progressbar(0, current_progress, PRN_MAX_PROGRESS);
                    }
                }
                remote_activate(REMOTE_ENABLED);
                gbprinter_set_handler(onPrinterProgress, BANK(state_gallery));
                JOYPAD_RESET();
                break;
            case ACTION_PRINT_INFO:
                remote_activate(REMOTE_DISABLED);
                if (!gallery_print_info()) {
                    music_play_sfx(BANK(sound_error), sound_error, SFX_MUTE_MASK(sound_error), MUSIC_SFX_PRIORITY_MINIMAL);
                }
                remote_activate(REMOTE_ENABLED);
                JOYPAD_RESET();
                break;
            default:
                // error, must not get here
                music_play_sfx(BANK(sound_error), sound_error, SFX_MUTE_MASK(sound_error), MUSIC_SFX_PRIORITY_MINIMAL);
                break;
        }
        OPTION(gallery_picture_idx) = refresh_screen();
    } else if (KEY_PRESSED(J_START)) {
        // run Main Menu
        if (!menu_main_execute()) refresh_screen();
    }
    return TRUE;
}

uint8_t LEAVE_state_gallery() BANKED {
    fade_out_modal();
    gbprinter_set_handler(NULL, 0);
    return 0;
}
