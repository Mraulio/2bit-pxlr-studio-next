#pragma bank 255

#include <gbdk/platform.h>
#include <stdint.h>

#include "systemdetect.h"
#include "sgb_border.h"
#include "sgb_assets.h"

#include "state_camera.h"

#include "camera_sgb_border.h"
#include "camera_sgb_border_default.h"

BANKREF(module_sgb_assets)

static const uint8_t sgb_palette_gray[] = {
    SGB_PKT(SGB_PAL_01),
    SNES_LE(SNES_WHITE), SNES_LE(SNES_LIGHTGRAY), SNES_LE(SNES_DARKGRAY), SNES_LE(SNES_BLACK),
                         SNES_LE(SNES_LIGHTGRAY), SNES_LE(SNES_DARKGRAY), SNES_LE(SNES_BLACK)
};
static const uint8_t sgb_palette_red[] = {
    SGB_PKT(SGB_PAL_01),
    SNES_LE(RGB8(0xf3, 0xf8, 0xc3)), SNES_LE(RGB8(0xd1, 0xe4, 0x10)), SNES_LE(RGB8(0xe4, 0x3d, 0x20)), SNES_LE(SNES_BLACK),
                                     SNES_LE(SNES_LIGHTGRAY),         SNES_LE(SNES_DARKGRAY),          SNES_LE(SNES_BLACK)
};

static const uint8_t * const sgb_palettes[] = { sgb_palette_gray, sgb_palette_red };

void sgb_assets_set_palette(uint8_t palette_index) {
    sgb_transfer((uint8_t *)sgb_palettes[palette_index]);
}

typedef struct border_descriptor_t {
    const uint8_t * tiles;
    size_t tiles_size;
    const uint8_t * map;
    size_t map_size;
    const uint8_t * palettes;
    size_t palettes_size;
    uint8_t bank;
} border_descriptor_t;

static const border_descriptor_t sgb_borders[] = {
    {
        .tiles = camera_sgb_border_default_tiles,       .tiles_size = sizeof(camera_sgb_border_default_tiles),
        .map = camera_sgb_border_default_map,           .map_size = sizeof(camera_sgb_border_default_map),
        .palettes = camera_sgb_border_default_palettes, .palettes_size = sizeof(camera_sgb_border_default_palettes),
        .bank = BANK(camera_sgb_border_default)
    }, {
        .tiles = camera_sgb_border_tiles,               .tiles_size = sizeof(camera_sgb_border_tiles),
        .map = camera_sgb_border_map,                   .map_size = sizeof(camera_sgb_border_map),
        .palettes = camera_sgb_border_palettes,         .palettes_size = sizeof(camera_sgb_border_palettes),
        .bank = BANK(camera_sgb_border)
    }
};

void sgb_assets_set_border(uint8_t border_index) {
    set_sgb_border(sgb_borders[border_index].tiles,     sgb_borders[border_index].tiles_size,
                   sgb_borders[border_index].map,       sgb_borders[border_index].map_size,
                   sgb_borders[border_index].palettes,  sgb_borders[border_index].palettes_size,
                   sgb_borders[border_index].bank);
}

// load the SGB borders and palettes if SGB detected
uint8_t INIT_module_sgb_assets() BANKED {
    if (_is_SUPER) {
        if (OPTION(fancy_sgb_border)) {
            sgb_assets_set_border(SGB_BORDER_FANCY);
            sgb_assets_set_palette(SGB_PALETTE_RED);
        } else {
            sgb_assets_set_border(SGB_BORDER_DEFAULT);
            sgb_assets_set_palette(SGB_PALETTE_GRAY);
        }
    }
    return 0;
}