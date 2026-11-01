#include "check.h"
#include "keymap.h"
#include "led_effect.h"
#include "vialrgb.h"
#include <string.h>

// The profile's settings, which is what the Lighting tab reads and writes
static uint16_t cfg[KEYMAP_CFG_COUNT];
static unsigned saves;

uint16_t keymap_cfg_get(uint8_t field)
{
    return (field < KEYMAP_CFG_COUNT) ? cfg[field] : 0;
}

bool keymap_cfg_set(uint8_t field, uint16_t value)
{
    cfg[field] = value;
    return true;
}

void vialrgb_hook_save(void)
{
    saves++;
}

static uint8_t packet[32];

static void get(uint8_t command, uint8_t arg0)
{
    memset(packet, 0, sizeof packet);
    packet[0] = 0x08; // id_lighting_get_value
    packet[1] = command;
    packet[2] = arg0;

    vialrgb_get_value(packet);
}

static void set_mode(uint8_t id, uint8_t speed, uint8_t hue, uint8_t sat, uint8_t val)
{
    memset(packet, 0, sizeof packet);
    packet[0] = 0x07; // id_lighting_set_value
    packet[1] = 0x41; // vialrgb_set_mode
    packet[2] = id;
    packet[3] = 0;
    packet[4] = speed;
    packet[5] = hue;
    packet[6] = sat;
    packet[7] = val;

    vialrgb_set_value(packet);
}

static void reset(void)
{
    memset(cfg, 0, sizeof cfg);
    saves = 0;
    vialrgb_init();
}

static void test_the_info_the_app_asks_for_first(void)
{
    reset();
    get(0x40, 0);

    CHECK(packet[2] == 1 && packet[3] == 0); // protocol 1: what this app demands
    CHECK(packet[4] == 255);                 // the brightest value it may send
}

static void test_the_effects_it_offers(void)
{
    reset();

    get(0x42, 0xFF); // "the ids above this": none left
    CHECK(packet[2] == 0xFF && packet[3] == 0xFF);

    get(0x42, 0); // the app walks up from zero: it must get a list that ends
    CHECK(packet[2] == 2 && packet[3] == 0);  // solid colour
    CHECK(packet[4] == 4 && packet[5] == 0);  // gradient up down
    CHECK(packet[6] == 5 && packet[7] == 0);  // gradient left right
    CHECK(packet[8] == 6 && packet[9] == 0);  // breathing
    CHECK(packet[10] == 18 && packet[11] == 0); // cycle out in (the rings)
    CHECK(packet[12] == 0xFF && packet[13] == 0xFF); // and the end
}

static void test_every_effect_has_an_id_and_back(void)
{
    reset();

    CHECK(vialrgb_id_of(FX_SOLID) == 2 && vialrgb_effect_of(2) == FX_SOLID);
    CHECK(vialrgb_id_of(FX_VERTICAL) == 4 && vialrgb_effect_of(4) == FX_VERTICAL);
    CHECK(vialrgb_id_of(FX_HORIZONTAL) == 5 && vialrgb_effect_of(5) == FX_HORIZONTAL);
    CHECK(vialrgb_id_of(FX_BREATHE) == 6 && vialrgb_effect_of(6) == FX_BREATHE);
    CHECK(vialrgb_id_of(FX_RADIAL) == 18 && vialrgb_effect_of(18) == FX_RADIAL);
    CHECK(vialrgb_id_of(FX_OFF) == 0 && vialrgb_effect_of(0) == FX_OFF);

    // the two without a counterpart in the app's list are reported as the nearest, and an unknown id is "off"
    CHECK(vialrgb_id_of(FX_PULSE) == 6 && vialrgb_id_of(FX_DIAGONAL) == 5);
    CHECK(vialrgb_effect_of(99) == FX_OFF);
}

static void test_the_mode_reads_and_writes_the_profile(void)
{
    reset();
    cfg[CFG_LED_EFFECT] = FX_HORIZONTAL;
    cfg[CFG_LED_SPEED]  = 7;
    cfg[CFG_LED_BRIGHT] = 128;
    cfg[CFG_LED_COLOR]  = 9; // the wheel at (9 - 1) * 16 = 128

    get(0x41, 0);
    CHECK(packet[2] == 5);   // the id of the effect in use
    CHECK(packet[4] == 7);   // speed
    CHECK(packet[5] == 128); // hue
    CHECK(packet[6] == 255); // saturated
    CHECK(packet[7] == 128); // brightness

    cfg[CFG_LED_COLOR] = 0; // white
    get(0x41, 0);
    CHECK(packet[5] == 0 && packet[6] == 0); // no hue, no saturation
}

static void test_choosing_from_the_app(void)
{
    reset();

    set_mode(4, 3, 0, 255, 200); // gradient up down, red, bright
    CHECK(cfg[CFG_LED_EFFECT] == FX_VERTICAL);
    CHECK(cfg[CFG_LED_SPEED] == 3 && cfg[CFG_LED_BRIGHT] == 200);
    CHECK(cfg[CFG_LED_COLOR] == 1); // the first colour of the wheel

    set_mode(0, 3, 0, 0, 200); // off
    CHECK(cfg[CFG_LED_EFFECT] == FX_OFF);

    set_mode(2, 3, 0, 0, 200); // solid, white
    CHECK(cfg[CFG_LED_EFFECT] == FX_SOLID && cfg[CFG_LED_COLOR] == 0);

    set_mode(99, 3, 0, 255, 200); // an id this keyboard does not have: off, rather than something random
    CHECK(cfg[CFG_LED_EFFECT] == FX_OFF);
}

static void test_a_colour_comes_back_the_same(void)
{
    reset();

    for (uint8_t step = 0; step < 17; step++) {
        const uint8_t hue = (uint8_t)(step * 16);

        set_mode(2, 3, hue, 255, 200);
        get(0x41, 0);

        CHECK(packet[5] == hue); // the same hue goes back to the app
    }
}

static void test_saving_writes_it_down(void)
{
    reset();
    vialrgb_save();
    CHECK(saves == 1);

    memset(packet, 0, sizeof packet);
    packet[0] = 0x07;
    packet[1] = 0x42; // a sub-command we do not have
    vialrgb_set_value(packet);
    CHECK(packet[0] == 0xFF); // told unhandled, as VIA expects
}

int main(void)
{
    test_the_info_the_app_asks_for_first();
    test_the_effects_it_offers();
    test_every_effect_has_an_id_and_back();
    test_the_mode_reads_and_writes_the_profile();
    test_choosing_from_the_app();
    test_a_colour_comes_back_the_same();
    test_saving_writes_it_down();
    return tests_result();
}
