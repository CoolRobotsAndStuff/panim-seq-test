#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <raylib.h>
#include <raymath.h>
#include "env.h"
#include "plug.h"
#define SEQ_CUSTOM_TIME
#define SEQ_IMPLEMENTATION
#include "seq.h"

#define PLUG(name, ret, ...) ret name(__VA_ARGS__);
LIST_OF_PLUGS
#undef PLUG

#define FONT_SIZE 68

typedef struct {
    size_t size;
    Font font;
    SeqThread thread1;
    SeqThread thread2;
    SeqThread sound_thread;
    Camera2D camera;
    Sound kick_sound;
    Wave kick_wave;
    Sound soft_sound;
    Wave soft_wave;
    bool finished;
    double global_time;
} Plug;

static Plug *p;

static void load_assets(void)
{
    p->font = LoadFontEx("./assets/fonts/Vollkorn-Regular.ttf", FONT_SIZE, NULL, 0);
    p->kick_wave = LoadWave("./assets/sounds/kick.wav");
    p->kick_sound = LoadSoundFromWave(p->kick_wave);

    p->soft_wave = LoadWave("./assets/sounds/plant-bomb.wav");
    p->soft_sound = LoadSoundFromWave(p->soft_wave);
}

static void unload_assets(void)
{
    UnloadFont(p->font);
    UnloadSound(p->kick_sound);
    UnloadWave(p->kick_wave);
    UnloadSound(p->soft_sound);
    UnloadWave(p->soft_wave);
}

void plug_reset(void)
{
    p->global_time = 0.0;
    p->camera = (Camera2D){
        .zoom=1.0
    };

    seq_current_thread = &p->sound_thread;
    seq_always_reset();
    seq_current_thread = &p->thread1;
    seq_always_reset();
    seq_current_thread = &p->thread2;
    seq_always_reset();
    p->finished = false;
}

void plug_init(void)
{
    p = malloc(sizeof(*p));
    assert(p != NULL);
    memset(p, 0, sizeof(*p));
    p->size = sizeof(*p);
    
    p->thread1 = seq_thread();
    p->thread2 = seq_thread();
    p->sound_thread = seq_thread();
    
    load_assets();
    plug_reset();
}

void *plug_pre_reload(void)
{
    unload_assets();
    return p;
}

void plug_post_reload(void *state)
{
    p = state;
    if (p->size < sizeof(*p)) {
        TraceLog(LOG_INFO, "Migrating plug state schema %zu bytes -> %zu bytes", p->size, sizeof(*p));
        p = realloc(p, sizeof(*p));
        p->size = sizeof(*p);
    }

    load_assets();
}

#define seq_do_for(s) if seq_sleep(s)

bool seq_lerp(float seconds, float* var, float from, float to) {
    if (seq_sleep(seconds)) {
        *var = ((float)seq_current_thread->delay_progress)/seq_current_thread->delay_amount * (to-from) + from;
        return true;
    }
    return false;
}

bool seq_squerp(float seconds, float* var, float from, float to) {
    if (seq_sleep(seconds)) {
        *var = pow(((float)seq_current_thread->delay_progress)/seq_current_thread->delay_amount, 2)
        * (to-from) + from;
        return true;
    }
    return false;
}

#define seq_lerp_multi_start seq_sleep

void seq_lerp_multi(float* var, float from, float to) {
    if (seq_current_thread->previous_delay_active)
        *var = ((float)seq_current_thread->delay_progress)/seq_current_thread->delay_amount * (to-from) + from;
}

void seq_sqrt_multi(float* var, float from, float to) {
    if (seq_current_thread->previous_delay_active)
        *var = sqrt(((float)seq_current_thread->delay_progress)/seq_current_thread->delay_amount) * (to-from) + from;
}

void seq_squerp_multi(float* var, float from, float to) {
    if (seq_current_thread->previous_delay_active)
        *var = pow(((float)seq_current_thread->delay_progress)/seq_current_thread->delay_amount, 2) * (to-from) + from;
}


int64_t seq_get_time_ns() {
    return (int64_t)(p->global_time * 1000 * 1000 * 1000);
}

typedef struct {
    Vector2 position;
    float size;
    float roundness;
    Color color;
    float slowness;
} Thing;

void draw_thing(Thing thing) {
    Rectangle rec = {
        .x          = thing.position.x-thing.size/2,
        .y          = thing.position.y-thing.size/2,
        .width      = thing.size,
        .height     = thing.size,
    };

    DrawRectangleRounded(rec, thing.roundness, 30, thing.color);
}

void seq_sync_with(SeqThread* t) {
    seq_sync_both(seq_current_thread, t);
}

void plug_update(Env env) {
    #define kick_sound() env.play_sound(p->kick_sound, p->kick_wave);
    #define soft_sound() env.play_sound(p->soft_sound, p->soft_wave);
    #define T1 seq_current_thread = &p->thread1;
    #define T2 seq_current_thread = &p->thread2;
    #define TSND seq_current_thread = &p->sound_thread;


    Color background_color = GetColor(0x181818FF);
    Color green_color      = GetColor(0x73C936FF);
    Color red_color        = GetColor(0xF43841FF);

    p->global_time += env.delta_time;

    ClearBackground(background_color);

    p->camera.offset = (Vector2) {
        env.screen_width/2, env.screen_height/2
    };
    p->camera.zoom=1.0;

    float transition = 0.3f;

    TSND
        seq_start();

    T2 
        seq_start();
        seq p->camera.rotation = 0;
        Thing seqv(thing2) = (Thing){
            .position = {200, 0},
            .roundness = 1,
            .color = green_color,
            .slowness = 1,
        };
        seq soft_sound();
        seq_lerp(transition*thing2.slowness, &thing2.size, 0, 150);
        seq_sleep(0.4);

    T1 
        seq_start();
        seq p->camera.rotation = 0;
        Thing seqv(thing1) = (Thing){
            .position = {-200, 0},
            .roundness = 0,
            .color = red_color,
            .slowness = 1.5,
        };
        seq_sleep(0.25);

        seq_sync_all((SeqThread*[]){ 
            &p->thread1,
            &p->thread2,
            &p->sound_thread
        }, 3);

        seq_lerp_multi_start(transition*thing1.slowness);
            seq_lerp_multi(&thing1.size, 0, 200);
            seq_lerp_multi(&thing2.roundness, 1, .5);
        seq soft_sound();
        seq_sleep(0.25);

    TSND
        seq_sync_with(&p->thread1);
        seq_sleep(transition*thing1.slowness-0.08);
        seq soft_sound();
    T1
        seq_squerp(transition*thing1.slowness, &thing1.roundness, 0, .5);
        seq_sleep(0.20);

    seq_sync_both(&p->thread1, &p->thread2);
    
    T1
        seq_sleep(0.30);

        float v1;
        if (seq_lerp(0.5, &v1, -20, 20)) thing1.position.y += v1;
        seq_lerp(0.3, &thing1.position.x, -200, 0);

    T2
        seq_sleep(0.10);
        float v2;
        if (seq_lerp(0.5, &v2, -30, 30)) thing2.position.y += v2;
        seq_sleep(0.1);
        seq_lerp_multi_start(0.3);
            seq_lerp_multi(&thing2.position.x, 200, thing1.position.x);
            seq_squerp_multi(&thing2.position.y, 0, thing1.position.y-thing1.size/2-thing2.size/2);
        
        seq soft_sound();

        seq_sleep(0.25);
        float seqv(current_y) = thing2.position.y;
        seq_squerp(0.5, &thing2.position.y, current_y, 0);
        seq kick_sound();

    seq_sync_both(&p->thread1, &p->thread2);
        float seqv(current_x) = thing2.position.x;
        seq_squerp(0.3, &thing2.position.x, current_x, 0);
        seq kick_sound();
    
    seq_sync_both(&p->thread1, &p->thread2);
    seq p->finished = true;


    BeginMode2D(p->camera);
        draw_thing(thing1);
        draw_thing(thing2);

    EndMode2D();
}

bool plug_finished(void)
{
    return p->finished;
}
