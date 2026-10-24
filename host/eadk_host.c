/* Fausse calculatrice pour tester Tycoon sur PC.
 * - les touches viennent d'un script (env TYCOON_SCRIPT) : lignes « debut_ms fin_ms touche »,
 *   « snap ms nom » pour une capture, « end ms » pour finir ;
 * - chaque dessin est écrit dans TYCOON_OPS (rendu ensuite en PNG par render.py) ;
 * - le système de fichiers est gardé dans TYCOON_FS entre deux lancements. */
#include <eadk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint64_t t_ms;
static FILE *ops;
typedef struct { uint64_t a, b; int key; } hold_t;
static hold_t holds[4096];
static int nholds;
typedef struct { uint64_t t; char name[64]; } snap_t;
static snap_t snaps[256];
static int nsnaps, next_snap;
static uint64_t end_ms = 600000;
static uint8_t fs[4 + 32768 + 4 + 64];
static const uint32_t FS_SIZE = 32768;

static const struct { const char *n; int k; } NAMES[] = {
    {"left", 0}, {"up", 1}, {"down", 2}, {"right", 3}, {"ok", 4}, {"back", 5}, {"home", 6}, {"onoff", 8},
    {"shift", 12}, {"exe", 52}, {"0", 48}, {"1", 42}, {"2", 43}, {"3", 44}, {"4", 36}, {"5", 37},
    {"6", 38}, {"7", 30}, {"8", 31}, {"9", 32}};

static void save_fs(void) {
  const char *p = getenv("TYCOON_FS");
  if (!p) return;
  FILE *f = fopen(p, "wb");
  fwrite(fs, 1, sizeof fs, f);
  fclose(f);
}
static void at_end(void) {
  save_fs();
  if (ops) fprintf(ops, "END\n"), fclose(ops);
}
static void finish(void) { exit(0); }
uint8_t *host_storage(uint32_t *size) {
  *size = FS_SIZE;
  return fs + 4;
}
__attribute__((constructor)) static void setup(void) {
  uint32_t magic = 0xEE0BDDBA;
  memcpy(fs, &magic, 4);
  memcpy(fs + 4 + FS_SIZE, &magic, 4);
  const char *p = getenv("TYCOON_FS");
  if (p) {
    FILE *f = fopen(p, "rb");
    if (f) fread(fs, 1, sizeof fs, f), fclose(f);
  }
  atexit(at_end);
  ops = fopen(getenv("TYCOON_OPS") ? getenv("TYCOON_OPS") : "ops.txt", "w");
  const char *s = getenv("TYCOON_SCRIPT");
  FILE *f = s ? fopen(s, "r") : NULL;
  char line[256];
  while (f && fgets(line, sizeof line, f)) {
    char a[64], b[64], c[64];
    int n = sscanf(line, "%63s %63s %63s", a, b, c);
    if (n >= 3 && !strcmp(a, "snap")) {
      snaps[nsnaps].t = strtoull(b, 0, 10);
      strcpy(snaps[nsnaps++].name, c);
    } else if (n >= 2 && !strcmp(a, "end")) {
      end_ms = strtoull(b, 0, 10);
    } else if (n == 3) {
      for (unsigned i = 0; i < sizeof NAMES / sizeof NAMES[0]; i++)
        if (!strcmp(c, NAMES[i].n)) {
          holds[nholds].a = strtoull(a, 0, 10), holds[nholds].b = strtoull(b, 0, 10);
          holds[nholds++].key = NAMES[i].k;
        }
    }
  }
  if (f) fclose(f);
}

eadk_keyboard_state_t eadk_keyboard_scan() {
  eadk_keyboard_state_t s = 0;
  for (int i = 0; i < nholds; i++)
    if (t_ms >= holds[i].a && t_ms < holds[i].b) s |= 1ull << holds[i].key;
  return s;
}
void eadk_timing_msleep(uint32_t ms) {
  t_ms += ms;
  while (next_snap < nsnaps && t_ms >= snaps[next_snap].t) fprintf(ops, "SNAP %s\n", snaps[next_snap++].name);
  if (t_ms >= end_ms) finish();
}
void eadk_timing_usleep(uint32_t us) { eadk_timing_msleep(us / 1000 ? us / 1000 : 1); }
uint64_t eadk_timing_millis() { return t_ms; }
uint32_t eadk_random() { return (uint32_t)rand() * 2654435761u ^ (uint32_t)rand(); }
void eadk_display_push_rect_uniform(eadk_rect_t r, eadk_color_t c) {
  fprintf(ops, "F %d %d %d %d %d\n", r.x, r.y, r.width, r.height, c);
}
void eadk_display_push_rect(eadk_rect_t r, const eadk_color_t *px) {
  fprintf(ops, "P %d %d %d %d", r.x, r.y, r.width, r.height);
  for (int i = 0; i < r.width * r.height; i++) fprintf(ops, " %d", px[i]);
  fprintf(ops, "\n");
}
void eadk_display_draw_string(const char *t, eadk_point_t p, bool big, eadk_color_t fg, eadk_color_t bg) {
  fprintf(ops, "S %d %d %d %d %d %s\n", p.x, p.y, big, fg, bg, t);
}
bool eadk_display_wait_for_vblank() { return true; }
void eadk_backlight_set_brightness(uint8_t b) { (void)b; }
uint8_t eadk_backlight_brightness() { return 255; }
