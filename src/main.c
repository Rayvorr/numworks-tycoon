/* Tycoon : un jeu de gestion pour la calculatrice NumWorks.
 *
 * Gagne de l'argent (EXE), achète des bâtiments qui rapportent tout seuls,
 * débloque des bonus, attrape les clients mystère et vends ton entreprise
 * à des investisseurs pour repartir plus fort.
 *
 * La partie est sauvegardée dans le fichier tycoon.sav (toutes les 20 s et en
 * quittant). Sauvegarde et touche Maison : epsilon_app.h et epsilon_files.h,
 * repris de NumPlay (Mason Chen, GPL-3.0). */
#include <eadk.h>
#include "epsilon_app.h"
#include "epsilon_files.h"

#ifdef HOST
#include <stdlib.h>
#endif
#ifndef HOST
const char eadk_app_name[] __attribute__((section(".rodata.eadk_app_name"))) = "Tycoon";
const uint32_t eadk_api_level __attribute__((section(".rodata.eadk_api_level"))) = 0;
#endif

/* ------------------------------------------------------------ couleurs */
#define RGB(r, g, b) ((eadk_color_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3)))
#define C_HEAD RGB(0x1E, 0x3A, 0x2F)
#define C_HEAD2 RGB(0x2F, 0x5A, 0x48)
#define C_GOLD RGB(0xFF, 0xD3, 0x4E)
#define C_MINT RGB(0xA8, 0xE6, 0xB8)
#define C_BG RGB(0xF4, 0xEF, 0xE4)
#define C_CARD RGB(0xFF, 0xFF, 0xFF)
#define C_SEL RGB(0xFF, 0xF1, 0xC4)
#define C_LINE RGB(0xDD, 0xD5, 0xC4)
#define C_TEXT RGB(0x22, 0x22, 0x22)
#define C_GREY RGB(0x80, 0x80, 0x80)
#define C_OK RGB(0x1F, 0x8A, 0x4C)
#define C_NO RGB(0xC0, 0x39, 0x2B)
#define C_EVENT RGB(0xC2, 0x3B, 0x22)
#define C_ORANGE RGB(0xFF, 0x9A, 0x2E)
#define C_WHITE 0xFFFF

#define KEY(k) (1ull << (k))
#define LW 10 /* grande police : 10 x 18 */
#define SW 7  /* petite police : 7 x 14 */

#define HEAD_H 40
#define TAB_Y 40
#define TAB_H 22
#define LIST_Y 62
#define ROW_H 32
#define ROWS 5
#define FOOT_Y 222

/* ------------------------------------------------------------ petits outils */
static void fill(int x, int y, int w, int h, eadk_color_t c) {
  if (w <= 0 || h <= 0) return;
  eadk_display_push_rect_uniform((eadk_rect_t){(uint16_t)x, (uint16_t)y, (uint16_t)w, (uint16_t)h}, c);
}
static void text(const char *s, int x, int y, bool big, eadk_color_t fg, eadk_color_t bg) {
  eadk_display_draw_string(s, (eadk_point_t){(uint16_t)x, (uint16_t)y}, big, fg, bg);
}
static int ulen(const char *s) { /* nombre de lettres (UTF-8) */
  int n = 0;
  for (; *s; s++)
    if (((uint8_t)*s & 0xC0) != 0x80) n++;
  return n;
}
static char *scat(char *p, const char *s) {
  while (*s) *p++ = *s++;
  *p = 0;
  return p;
}
static char *put_uint(char *p, uint32_t v) {
  char t[12];
  int n = 0;
  do t[n++] = (char)('0' + v % 10), v /= 10;
  while (v);
  while (n) *p++ = t[--n];
  *p = 0;
  return p;
}
/* s complété par des espaces jusqu'à n lettres (à droite, ou à gauche) */
static void pad(char *dst, const char *s, int n, bool left) {
  int l = ulen(s);
  char *p = dst;
  if (left)
    for (int i = l; i < n; i++) *p++ = ' ';
  p = scat(p, s);
  if (!left)
    for (int i = l; i < n; i++) *p++ = ' ';
  *p = 0;
}
static uint32_t hash(const void *d, int n, uint32_t h) {
  const uint8_t *b = (const uint8_t *)d;
  for (int i = 0; i < n; i++) h = (h ^ b[i]) * 16777619u;
  return h;
}
static uint32_t hstr(const char *s, uint32_t h) {
  while (*s) h = (h ^ (uint8_t)*s++) * 16777619u;
  return (h ^ 0xFF) * 16777619u;
}
static void zero(void *d, int n) {
  uint8_t *b = (uint8_t *)d;
  while (n--) *b++ = 0;
}
static double dpow(double b, uint32_t e) {
  double r = 1;
  while (e) {
    if (e & 1) r *= b;
    b *= b;
    e >>= 1;
  }
  return r;
}
static double dsqrt(double x) {
  if (x <= 0) return 0;
  double r = x > 1 ? x / 2 : 1;
  for (int i = 0; i < 60; i++) r = (r + x / r) / 2;
  return r;
}
static uint32_t rnd(uint32_t n) { return eadk_random() % n; }

/* Nombres lisibles : 999, 1,23 K, 45,6 M, 789 B... */
static const char *const SUF[] = {"", "K", "M", "B", "T", "Qa", "Qi", "Sx", "Sp", "Oc", "No", "Dc"};
static void fmt(double v, char *out) {
  char *p = out;
  if (v < 0) v = 0;
  if (v < 1000) {
    if (v < 100 && v - (double)(uint32_t)v >= 0.05) {
      uint32_t t = (uint32_t)(v * 10 + 0.5);
      p = put_uint(p, t / 10);
      if (t % 10) *p++ = ',', *p++ = (char)('0' + t % 10), *p = 0;
    } else {
      put_uint(p, (uint32_t)v);
    }
    return;
  }
  int i = 0;
  while (v >= 1000 && i < 11) v /= 1000, i++;
  if (v >= 1e9) v = 1e9 - 1;
  if (v < 10) {
    uint32_t s = (uint32_t)(v * 100);
    p = put_uint(p, s / 100);
    if (s % 100) {
      *p++ = ',', *p++ = (char)('0' + s / 10 % 10);
      if (s % 10) *p++ = (char)('0' + s % 10);
    }
  } else if (v < 100) {
    uint32_t s = (uint32_t)(v * 10);
    p = put_uint(p, s / 10);
    if (s % 10) *p++ = ',', *p++ = (char)('0' + s % 10);
  } else {
    p = put_uint(p, (uint32_t)v);
  }
  *p++ = ' ';
  scat(p, SUF[i]);
}

/* ------------------------------------------------------------ données du jeu */
#define NB 9
typedef struct {
  const char *name;
  double cost, inc;
  eadk_color_t tile;
} bdef_t;
static const bdef_t B[NB] = {
    {"Limonade", 15, 1, RGB(0xFF, 0xF6, 0xB8)},
    {"Boulangerie", 100, 3, RGB(0xF8, 0xE2, 0xC4)},
    {"Pizzeria", 1100, 20, RGB(0xFF, 0xDC, 0xC8)},
    {"Cinéma", 12000, 110, RGB(0xF2, 0xC8, 0xC8)},
    {"Usine", 130000, 700, RGB(0xD8, 0xE4, 0xEE)},
    {"Banque", 1.4e6, 4000, RGB(0xE4, 0xF2, 0xDC)},
    {"Studio jeux", 2e7, 25000, RGB(0xE8, 0xDC, 0xF6)},
    {"Fusée", 3.3e8, 150000, RGB(0xD4, 0xE4, 0xFF)},
    {"Base martienne", 5.1e9, 1e6, RGB(0xFF, 0xE0, 0xD0)},
};
static const uint16_t MILESTONES[] = {25, 50, 100, 200, 300, 400, 500};
#define NMS ((int)(sizeof MILESTONES / sizeof MILESTONES[0]))

enum { UP_B, UP_CLICK, UP_ALL, UP_SUPER };
#define NU 36
typedef struct {
  double cost;
  uint8_t type, target, mult10, id;
  char name[28];
} up_t;
static up_t U[NU]; /* rangés du moins cher au plus cher ; id = bit dans la sauvegarde */

static void add_up(int *n, double cost, int type, int target, int mult10, const char *a, const char *b) {
  up_t *u = &U[*n];
  u->cost = cost, u->type = (uint8_t)type, u->target = (uint8_t)target, u->mult10 = (uint8_t)mult10;
  u->id = (uint8_t)*n;
  char *p = scat(u->name, a);
  if (b) scat(p, b);
  (*n)++;
}
static void init_upgrades(void) {
  int n = 0;
  static const char *const TIER[] = {" Pro", " Deluxe", " Ultime"};
  static const double TCOST[] = {25, 2500, 250000};
  static const int TMULT[] = {20, 30, 40};
  for (int b = 0; b < NB; b++)
    for (int t = 0; t < 3; t++) add_up(&n, B[b].cost * TCOST[t], UP_B, b, TMULT[t], B[b].name, TIER[t]);
  add_up(&n, 100, UP_CLICK, 0, 20, "Gants de travail", 0);
  add_up(&n, 5000, UP_CLICK, 0, 30, "Clic turbo", 0);
  add_up(&n, 50000, UP_SUPER, 0, 0, "Super clic", 0);
  add_up(&n, 5e6, UP_CLICK, 0, 50, "Clic en or", 0);
  add_up(&n, 5e9, UP_CLICK, 0, 100, "Clic cosmique", 0);
  add_up(&n, 1e6, UP_ALL, 0, 15, "Publicité", 0);
  add_up(&n, 5e8, UP_ALL, 0, 20, "Pub à la télé", 0);
  add_up(&n, 5e10, UP_ALL, 0, 20, "Influenceurs", 0);
  add_up(&n, 5e12, UP_ALL, 0, 30, "Monopole", 0);
  for (int i = 1; i < n; i++) { /* tri par prix (stable) */
    up_t t = U[i];
    int j = i - 1;
    while (j >= 0 && U[j].cost > t.cost) U[j + 1] = U[j], j--;
    U[j + 1] = t;
  }
}

/* ------------------------------------------------------------ état (sauvegardé) */
#define SAVE_NAME "tycoon.sav"
#define SAVE_VERSION 1
static struct {
  uint8_t magic, version, buymode, unused;
  uint32_t playsec, achieved;
  uint32_t unused2;
  double money, run, total, investors;
  uint64_t ups;
  uint16_t count[NB];
  uint16_t unused3;
} S;

static void new_game(void) {
  zero(&S, sizeof S);
  S.magic = 'T', S.version = SAVE_VERSION;
}
static void save(void) {
  S.magic = 'T', S.version = SAVE_VERSION;
  ef_write(SAVE_NAME, &S, sizeof S);
}
static bool load(void) {
  uint32_t len = 0;
  const uint8_t *d = ef_read(SAVE_NAME, &len);
  if (!d || len != sizeof S || d[0] != 'T' || d[1] != SAVE_VERSION) return false;
  uint8_t *s = (uint8_t *)&S;
  for (uint32_t i = 0; i < len; i++) s[i] = d[i];
  if (S.buymode > 2) S.buymode = 0;
  if (!(S.money >= 0)) S.money = 0; /* (aussi NaN) */
  return true;
}
static bool has_up(int id) { return (S.ups >> id) & 1; }

/* ------------------------------------------------------------ état (partie en cours) */
static uint64_t now_ms;
static double boost = 1;
static uint64_t boost_until, next_event, event_end, toast_until, confirm_until;
static int event_digit; /* 0 : pas de client mystère */
static char toast[64];
static eadk_color_t toast_bg;
static int tab, sel[3], scroll[3];
static double income, click_gain;

static void say(const char *s, eadk_color_t bg, int ms) {
  pad(toast, s, 0, false);
  toast_bg = bg;
  toast_until = now_ms + (uint64_t)ms;
}

static double bmult(int b) {
  double m = 1;
  for (int k = 0; k < NMS; k++)
    if (S.count[b] >= MILESTONES[k]) m *= 2;
  for (int i = 0; i < NU; i++)
    if (U[i].type == UP_B && U[i].target == b && has_up(U[i].id)) m *= U[i].mult10 / 10.0;
  return m;
}
static double gmult(void) {
  double m = 1 + 0.02 * S.investors;
  for (int i = 0; i < NU; i++)
    if (U[i].type == UP_ALL && has_up(U[i].id)) m *= U[i].mult10 / 10.0;
  return m;
}
static double unit_income(int b) { return B[b].inc * bmult(b) * gmult(); }
static void compute_rates(void) {
  double g = gmult(), sum = 0;
  for (int b = 0; b < NB; b++) sum += S.count[b] * B[b].inc * bmult(b);
  income = sum * g * boost;
  double c = 1;
  bool super = false;
  for (int i = 0; i < NU; i++) {
    if (!has_up(U[i].id)) continue;
    if (U[i].type == UP_CLICK) c *= U[i].mult10 / 10.0;
    if (U[i].type == UP_SUPER) super = true;
  }
  click_gain = c * g * boost + (super ? income * 0.02 : 0);
}
static void earn(double v) {
  S.money += v, S.run += v, S.total += v;
}
static double investors_gain(void) {
  double g = 10 * dsqrt(S.run / 1e9);
  return g < 1 ? 0 : (double)(uint32_t)g;
}

/* prix de n bâtiments b à partir de ceux qu'on a déjà */
static double price_n(int b, uint32_t n) {
  double first = B[b].cost * dpow(1.15, S.count[b]);
  return first * (dpow(1.15, n) - 1) / 0.15;
}
static uint32_t max_affordable(int b, double *total) {
  double p = B[b].cost * dpow(1.15, S.count[b]), t = 0;
  uint32_t n = 0;
  while (n < 1000 && t + p <= S.money) t += p, p *= 1.15, n++;
  *total = t;
  return n;
}
/* combien on achèterait (0 : pas assez d'argent) et pour combien */
static uint32_t buy_plan(int b, double *cost) {
  if (S.buymode == 0) return (*cost = price_n(b, 1)) <= S.money ? 1 : 0;
  if (S.buymode == 1) return (*cost = price_n(b, 10)) <= S.money ? 10 : 0;
  uint32_t n = max_affordable(b, cost);
  if (!n) *cost = price_n(b, 1);
  return n;
}

/* ------------------------------------------------------------ succès */
static void check_achievements(void) {
  static const double AT[] = {1e3, 1e6, 1e9, 1e12, 1e15};
  static const char *const MSG[] = {"Premier millier de $ !", "Bravo, tu es millionnaire !", "Milliardaire ! Quel empire !",
                                    "Mille milliards de $ !!!", "Légende du business !"};
  for (int i = 0; i < 5; i++)
    if (!(S.achieved >> i & 1) && S.total >= AT[i]) {
      S.achieved |= 1u << i;
      say(MSG[i], C_OK, 3500);
    }
}

/* ------------------------------------------------------------ icônes (12 x 12, dessinées en double) */
#define IC_CLICK 9
#define IC_STAR 10
#define IC_LOCK 11
#define IC_CASE 12
static const char *const ICONS[13][12] = {
    {"........s...", ".......s....", "..bbbbsbbb..", "..byyysyyb..", "..byyysyyb..", "..byyyyyyb..",
     "...byyyyb...", "...byyyyb...", "...byyyyb...", "....bbbb....", "....bbbb....", "...bbbbbb..."},
    {"............", "............", "...mmmmmm...", "..mlmlmlmm..", ".mmlmlmlmmm.", ".mmmmmmmmmm.",
     ".mmmmmmmmmm.", ".ommmmmmmmo.", "..oooooooo..", "............", "............", "............"},
    {"............", ".cccccccccc.", ".cyyyyyyyyc.", "..yyryyyry..", "..yrrryyyy..", "...yryyyy...",
     "...yyyyry...", "....yyrr....", "....yyyy....", ".....yy.....", ".....yy.....", "............"},
    {"............", ".kwkkwkkwkk.", ".kkwkkwkkwk.", "............", ".kkkkkkkkkk.", ".kwwwwwwwwk.",
     ".kwkkkkkkwk.", ".kwkkkkkkwk.", ".kwwwwwwwwk.", ".kkkkkkkkkk.", "............", "............"},
    {".......ss...", "......ss.ss.", "........dd..", "........dd..", "........dd..", ".g..g...dd..",
     ".gg.gg..dd..", ".ggggggggdd.", ".gyggyggydd.", ".ggggggggdd.", ".gyggyggydd.", ".dddddddddd."},
    {".....yy.....", "....yyyy....", "...yyyyyy...", "..yyyyyyyy..", ".yyyyyyyyyy.", "..w.w..w.w..",
     "..w.w..w.w..", "..w.w..w.w..", "..w.w..w.w..", ".yyyyyyyyyy.", "yyyyyyyyyyyy", "............"},
    {"............", "............", "............", "..pppppppp..", ".pppppppppp.", ".pwpppppkpp.",
     ".wwwpppkpkp.", ".pwpppppkpp.", ".pppppppppp.", ".ppp....ppp.", "..p......p..", "............"},
    {".....rr.....", "....rwwr....", "....wwww....", "...wwbbww...", "...wwbbww...", "...wwwwww...",
     "...wwwwww...", "..rwwwwwwr..", ".rrwwwwwwrr.", ".rr.oyyo.rr.", ".....oo.....", ".....o......"},
    {"............", "....cccc....", "...cwcccc...", "..cwcccccc..", "..cccccccc..", "..cccccccc..",
     "rrrrrrrrrrrr", "rdrrrrdrrrrr", "rrrrdrrrrdrr", "rrrrrrrrrrrr", "rdrrrrrdrrrr", "rrrrrrrrrrrr"},
    {"...oooooo...", "..oyyyyyyo..", ".oyyyoyyyyo.", ".oyyoooooyo.", ".oyoyoyyyyo.", ".oyyoooyyyo.",
     ".oyyyyoyoyo.", ".oyooooooyo.", ".oyyyyoyyyo.", "..oyyyyyyo..", "...oooooo...", "............"},
    {".....yy.....", ".....yy.....", "....yyyy....", "yyyyyyyyyyyy", ".yyyyyyyyyy.", "..yyyyyyyy..",
     "...yyyyyy...", "..yyyyyyyy..", "..yyy..yyy..", ".yyy....yyy.", ".yy......yy.", "............"},
    {"............", "....dddd....", "...d....d...", "...d....d...", "...d....d...", "..gggggggg..",
     "..gggddggg..", "..gggddggg..", "..ggggdggg..", "..gggggggg..", "..gggggggg..", "............"},
    {"............", "....mmmm....", "....m..m....", ".mmmmmmmmmm.", ".mllllllllm.", ".mllllllllm.",
     ".mmmmyymmmm.", ".mllllllllm.", ".mllllllllm.", ".mllllllllm.", ".mmmmmmmmmm.", "............"},
};
static const eadk_color_t ICON_TILE[13] = {0, 0, 0, 0, 0, 0, 0, 0, 0,
                                           RGB(0xFF, 0xF3, 0xC8), RGB(0xFF, 0xF3, 0xC8), RGB(0xE8, 0xE8, 0xE8),
                                           RGB(0xF0, 0xE4, 0xD4)};
static eadk_color_t pal(char c) {
  switch (c) {
    case 's': return RGB(0xE0, 0x30, 0x40);
    case 'b': return RGB(0x6A, 0xA8, 0xD8);
    case 'y': return RGB(0xF7, 0xC2, 0x2C);
    case 'm': return RGB(0xC0, 0x7A, 0x3A);
    case 'l': return RGB(0xE8, 0xB0, 0x70);
    case 'o': return RGB(0x8A, 0x4E, 0x1E);
    case 'c': return RGB(0xB0, 0x6A, 0x2A);
    case 'r': return RGB(0xD0, 0x40, 0x30);
    case 'k': return RGB(0x22, 0x22, 0x22);
    case 'w': return RGB(0xFF, 0xFF, 0xFF);
    case 'g': return RGB(0x9A, 0xA4, 0xAE);
    case 'd': return RGB(0x5A, 0x62, 0x6A);
    case 'p': return RGB(0x8E, 0x5C, 0xD0);
    default: return 0;
  }
}
static eadk_color_t icon_buf[24 * 24];
static void icon(int id, int x, int y) {
  eadk_color_t tile = id < NB ? B[id].tile : ICON_TILE[id];
  if (id == 8) tile = RGB(0x1A, 0x1A, 0x3A); /* le ciel de Mars */
  for (int r = 0; r < 12; r++)
    for (int c = 0; c < 12; c++) {
      char ch = ICONS[id][r][c];
      eadk_color_t col = ch == '.' ? tile : id == 8 && ch == 'c' ? RGB(0x7E, 0xD8, 0xE8) : pal(ch);
      if (id == 9 && ch == 'o') col = RGB(0xD0, 0x8A, 0x10);
      icon_buf[(2 * r) * 24 + 2 * c] = icon_buf[(2 * r) * 24 + 2 * c + 1] = col;
      icon_buf[(2 * r + 1) * 24 + 2 * c] = icon_buf[(2 * r + 1) * 24 + 2 * c + 1] = col;
    }
  eadk_display_push_rect((eadk_rect_t){(uint16_t)x, (uint16_t)y, 24, 24}, icon_buf);
}
/* une icône en grand (pixels de `s` x `s`), fond transparent */
static void big_icon(int id, int x, int y, int s) {
  for (int r = 0; r < 12; r++)
    for (int c = 0; c < 12; c++) {
      char ch = ICONS[id][r][c];
      if (ch == '.') continue;
      eadk_color_t col = pal(ch);
      if (id == 9 && ch == 'o') col = RGB(0xD0, 0x8A, 0x10);
      fill(x + c * s, y + r * s, s, s, col);
    }
}

/* ------------------------------------------------------------ listes */
/* lignes de l'onglet Bâtiments : les bâtiments débloqués, puis un « ??? » */
static int build_rows(int *ids) {
  int n = 0;
  for (int b = 0; b < NB; b++) {
    if (b == 0 || S.count[b - 1] > 0 || S.count[b] > 0) ids[n++] = b;
    else {
      ids[n++] = 100 + b; /* verrouillé */
      break;
    }
  }
  return n;
}
static int bonus_rows(int *ids) {
  int n = 0;
  for (int i = 0; i < NU; i++) {
    if (has_up(U[i].id)) continue;
    if (U[i].type == UP_B && S.count[U[i].target] == 0) continue;
    ids[n++] = i;
  }
  return n;
}
static int affordable_bonus(void) {
  int ids[NU], n = bonus_rows(ids), k = 0;
  for (int i = 0; i < n; i++)
    if (U[ids[i]].cost <= S.money) k++;
  return k;
}
static int list_rows(int *ids) { return tab == 0 ? build_rows(ids) : tab == 1 ? bonus_rows(ids) : 0; }

typedef struct {
  int icon;
  bool sel, wide;
  char l1[32], r1[12], l2[40], r2[24];
  eadk_color_t c1, cr1, c2, cr2;
} row_t;

static void row_content(int id, bool selected, row_t *r) {
  zero(r, sizeof *r);
  r->sel = selected;
  r->c1 = C_TEXT, r->cr1 = C_TEXT, r->c2 = C_GREY, r->cr2 = C_GREY;
  char a[24], *p;
  if (tab == 0) {
    if (id >= 100) {
      int b = id - 100;
      r->icon = IC_LOCK;
      scat(r->l1, "???");
      r->c1 = C_GREY;
      p = scat(r->l2, "Achète 1 ");
      scat(p, B[b - 1].name);
      return;
    }
    int b = id;
    r->icon = b;
    scat(r->l1, B[b].name);
    p = scat(r->r1, "x");
    put_uint(p, S.count[b]);
    r->cr1 = S.count[b] ? C_HEAD : C_GREY;
    double cost;
    uint32_t n = buy_plan(b, &cost);
    fmt(cost, a);
    if (S.buymode == 1) n = 10;
    if (S.buymode == 0 || n == 0) p = scat(r->l2, "Prix ");
    else {
      p = put_uint(r->l2, n);
      p = scat(p, " pour ");
    }
    p = scat(p, a);
    scat(p, " $");
    r->c2 = cost <= S.money ? C_OK : C_NO;
    if (S.count[b]) {
      fmt(S.count[b] * unit_income(b) * boost, a);
      p = scat(r->r2, "+");
      p = scat(p, a);
      scat(p, "/s");
    } else {
      fmt(unit_income(b) * boost, a);
      p = scat(r->r2, "+");
      p = scat(p, a);
      scat(p, "/s chacun");
    }
    /* prochain palier */
    /* presque au prochain palier : on l'annonce à la place du revenu */
    for (int k = 0; k < NMS; k++)
      if (S.count[b] < MILESTONES[k]) {
        if (S.count[b] + 5 >= MILESTONES[k]) {
          r->r2[0] = 0;
          p = scat(r->r2, "x2 à ");
          put_uint(p, MILESTONES[k]);
          r->cr2 = C_ORANGE;
        }
        break;
      }
  } else {
    up_t *u = &U[id];
    r->wide = true;
    r->icon = u->type == UP_B ? u->target : u->type == UP_ALL ? IC_STAR : IC_CLICK;
    scat(r->l1, u->name);
    fmt(u->cost, a);
    p = scat(r->l2, "Prix ");
    p = scat(p, a);
    scat(p, " $");
    r->c2 = u->cost <= S.money ? C_OK : C_NO;
    if (u->type == UP_SUPER) scat(r->r2, "clic +2% du revenu");
    else {
      p = scat(r->r2, u->type == UP_B ? B[u->target].name : u->type == UP_ALL ? "Tout" : "Clic");
      p = scat(p, " x");
      p = put_uint(p, u->mult10 / 10);
      if (u->mult10 % 10) *p++ = ',', *p++ = (char)('0' + u->mult10 % 10), *p = 0;
    }
  }
}

/* ------------------------------------------------------------ dessin */
static uint32_t h_head, h_badge, h_tabs, h_foot, h_panel;
static uint32_t h_row_struct[ROWS], h_row_text[ROWS];
static eadk_color_t foot_bg_drawn;

static void invalidate(void) {
  h_head = h_badge = h_tabs = h_foot = h_panel = 1;
  for (int i = 0; i < ROWS; i++) h_row_struct[i] = h_row_text[i] = 1;
  foot_bg_drawn = 1;
}

static void draw_header(void) {
  char a[24], s[40], t[64], *p;
  fmt(S.money, a);
  p = scat(s, a);
  scat(p, " $");
  pad(t, s, 15, false);
  uint32_t h = hstr(t, 7);
  fmt(income, a);
  p = scat(s, "+");
  p = scat(p, a);
  p = scat(p, " $/s   clic +");
  fmt(click_gain, a);
  p = scat(p, a);
  char t2[64];
  pad(t2, s, 33, false);
  h = hstr(t2, h);
  if (h != h_head) {
    h_head = h;
    text(t, 10, 2, true, C_GOLD, C_HEAD);
    text(t2, 10, 23, false, C_MINT, C_HEAD);
  }
  /* badge : quantité d'achat, et rush en cours */
  static const char *const MODE[] = {"x1", "x10", "MAX"};
  char b2[16] = "";
  if (boost > 1) {
    p = scat(b2, "RUSH ");
    p = put_uint(p, (uint32_t)((boost_until - now_ms) / 1000 + 1));
    scat(p, "s");
  }
  h = hstr(MODE[S.buymode], hstr(b2, 3));
  if (h != h_badge) {
    h_badge = h;
    fill(250, 0, 70, HEAD_H, C_HEAD);
    int w = ulen(MODE[S.buymode]) * SW + 10;
    fill(312 - w, 4, w, 17, C_GOLD);
    text(MODE[S.buymode], 317 - w, 6, false, C_HEAD, C_GOLD);
    if (b2[0]) text(b2, 312 - ulen(b2) * SW, 24, false, C_ORANGE, C_HEAD);
  }
}

static void draw_tabs(void) {
  char bonus[20], *p;
  int k = affordable_bonus();
  p = scat(bonus, "Bonus");
  if (k) {
    p = scat(p, " (");
    p = put_uint(p, (uint32_t)k);
    scat(p, ")");
  }
  const char *names[3] = {"Bâtiments", bonus, "Investir"};
  uint32_t h = hstr(bonus, (uint32_t)tab + 11);
  if (h == h_tabs) return;
  h_tabs = h;
  static const int X[4] = {0, 107, 213, 320};
  for (int i = 0; i < 3; i++) {
    bool on = i == tab;
    eadk_color_t bg = on ? C_BG : C_HEAD2, fg = on ? C_HEAD : C_MINT;
    fill(X[i], TAB_Y, X[i + 1] - X[i], TAB_H, bg);
    if (i) fill(X[i], TAB_Y, 1, TAB_H, C_HEAD);
    int w = ulen(names[i]) * SW;
    text(names[i], X[i] + (X[i + 1] - X[i] - w) / 2, TAB_Y + 4, false, (i == 1 && k && !on) ? C_GOLD : fg, bg);
  }
}

static void draw_row(int slot, const row_t *r) {
  int y = LIST_Y + slot * ROW_H;
  eadk_color_t bg = r->sel ? C_SEL : C_CARD;
  uint32_t hs = hash(&r->icon, sizeof r->icon, (uint32_t)r->sel * 31 + (uint32_t)r->wide + 5);
  char l1[40], r1[16], l2[48], r2[32];
  pad(l1, r->l1, r->wide ? 27 : 15, false);
  pad(r1, r->r1, 5, true);
  pad(l2, r->l2, 20, false);
  pad(r2, r->r2, 18, true);
  uint32_t ht = hstr(l1, hstr(r1, hstr(l2, hstr(r2, 9))));
  ht = hash(&r->c1, sizeof(eadk_color_t) * 4, ht);
  if (hs != h_row_struct[slot]) {
    h_row_struct[slot] = hs;
    h_row_text[slot] = 0;
    fill(0, y, 320, ROW_H - 1, bg);
    fill(0, y + ROW_H - 1, 320, 1, C_LINE);
    if (r->sel) fill(0, y, 4, ROW_H - 1, C_GOLD);
    icon(r->icon, 7, y + 4);
  }
  if (ht != h_row_text[slot]) {
    h_row_text[slot] = ht;
    text(l1, 38, y, true, r->c1, bg);
    if (!r->wide) text(r1, 312 - 5 * LW, y, true, r->cr1, bg);
    text(l2, 38, y + 17, false, r->c2, bg);
    text(r2, 312 - 18 * SW, y + 17, false, r->cr2, bg);
  }
}
static void clear_row(int slot) {
  if (h_row_struct[slot] == 0) return;
  h_row_struct[slot] = 0;
  h_row_text[slot] = 0;
  fill(0, LIST_Y + slot * ROW_H, 320, ROW_H, C_BG);
}

static void draw_list(void) {
  int ids[NU + NB], n = list_rows(ids);
  if (sel[tab] >= n) sel[tab] = n ? n - 1 : 0;
  if (sel[tab] < scroll[tab]) scroll[tab] = sel[tab];
  if (sel[tab] >= scroll[tab] + ROWS) scroll[tab] = sel[tab] - ROWS + 1;
  if (scroll[tab] > n - ROWS) scroll[tab] = n > ROWS ? n - ROWS : 0;
  row_t r;
  for (int s = 0; s < ROWS; s++) {
    int i = scroll[tab] + s;
    if (i < n) {
      row_content(ids[i], i == sel[tab], &r);
      draw_row(s, &r);
    } else if (tab == 1 && n == 0 && s == 0) {
      zero(&r, sizeof r);
      r.icon = IC_STAR, r.wide = true, r.c1 = C_GREY, r.c2 = C_GREY;
      scat(r.l1, "Pas de bonus pour l'instant");
      scat(r.l2, "Achète des bâtiments !");
      draw_row(0, &r);
    } else {
      clear_row(s);
    }
  }
}

static void draw_panel(void) {
  char a[24], lines[8][64], *p;
  eadk_color_t col[8];
  bool big[8];
  int n = 0;
  zero(lines, sizeof lines);
#define LINE(b, c) (big[n] = (b), col[n] = (c), lines[n++])
  p = scat(LINE(true, C_HEAD), "Investisseurs : ");
  fmt(S.investors, a);
  scat(p, a);
  p = scat(LINE(false, C_TEXT), "Bonus : +");
  fmt(S.investors * 2, a);
  p = scat(p, a);
  scat(p, " % sur tes revenus");
  scat(LINE(false, C_GREY), "Si tu vends ton entreprise maintenant :");
  double g = investors_gain();
  if (g >= 1) {
    p = scat(LINE(true, C_OK), "+");
    fmt(g, a);
    p = scat(p, a);
    scat(p, " investisseurs");
  } else {
    scat(LINE(true, C_GREY), "+0 investisseur");
  }
  if (g >= 1) {
    scat(LINE(false, C_TEXT), "Tu repars de zéro (argent, bâtiments,");
    scat(LINE(false, C_TEXT), "bonus) mais chaque investisseur donne");
    scat(LINE(false, C_TEXT), "+2 % de revenus pour toujours.");
    scat(LINE(false, C_HEAD), "OK : vendre l'entreprise");
  } else {
    scat(LINE(false, C_TEXT), "Gagne au moins 10 M $ dans cette partie");
    scat(LINE(false, C_TEXT), "pour attirer des investisseurs. Ils");
    scat(LINE(false, C_TEXT), "donnent +2 % de revenus pour toujours.");
    p = scat(LINE(false, C_GREY), "Gagné dans cette partie : ");
    fmt(S.run, a);
    p = scat(p, a);
    scat(p, " $");
  }
#undef LINE
  uint32_t h = 77;
  for (int i = 0; i < n; i++) h = hstr(lines[i], h);
  if (h == h_panel) return;
  if (h_panel == 1) {
    fill(0, LIST_Y, 320, FOOT_Y - LIST_Y, C_BG);
    big_icon(IC_CASE, 274, LIST_Y + 4, 3);
  }
  h_panel = h;
  int y = LIST_Y + 6;
  for (int i = 0; i < n; i++) {
    char t[80];
    pad(t, lines[i], big[i] ? 24 : i < 2 ? 35 : 43, false);
    text(t, 10, y, big[i], col[i], C_BG);
    y += big[i] ? 22 : 16;
    if (i == 1) y += 6;
  }
}

static void draw_footer(void) {
  char s[64], t[64], *p;
  eadk_color_t bg = C_HEAD, fg = C_WHITE;
  if (event_digit) {
    p = scat(s, "Client mystère ! Appuie vite sur ");
    p = put_uint(p, (uint32_t)event_digit);
    p = scat(p, " (");
    p = put_uint(p, (uint32_t)((event_end - now_ms) / 1000 + 1));
    scat(p, "s)");
    bg = C_EVENT, fg = C_GOLD;
  } else if (now_ms < toast_until) {
    scat(s, toast);
    bg = toast_bg, fg = C_WHITE;
  } else if (tab == 2) {
    scat(s, "OK:vendre  <>:onglets  Retour:quitter");
  } else {
    scat(s, "OK:acheter EXE:gagner shift:qté <>:onglets");
  }
  pad(t, s, 45, false);
  uint32_t h = hstr(t, bg);
  if (h == h_foot) return;
  h_foot = h;
  if (bg != foot_bg_drawn) {
    fill(0, FOOT_Y, 320, 240 - FOOT_Y, bg);
    foot_bg_drawn = bg;
  }
  text(t, 3, FOOT_Y + 2, false, fg, bg);
}

static void draw_all(bool full) {
  if (full) {
    invalidate();
    fill(0, 0, 320, HEAD_H, C_HEAD);
    fill(0, LIST_Y, 320, FOOT_Y - LIST_Y, tab == 2 ? C_BG : C_BG);
  }
  draw_header();
  draw_tabs();
  if (tab == 2) draw_panel();
  else draw_list();
  draw_footer();
}

/* ------------------------------------------------------------ actions */
static void buy_building(int b) {
  double cost;
  uint32_t n = buy_plan(b, &cost);
  if (!n) {
    say("Pas assez d'argent !", C_NO, 1500);
    return;
  }
  uint16_t before = S.count[b];
  S.money -= cost;
  S.count[b] = (uint16_t)(S.count[b] + n);
  for (int k = NMS - 1; k >= 0; k--)
    if (before < MILESTONES[k] && S.count[b] >= MILESTONES[k]) {
      char s[64], *p = scat(s, "Palier ");
      p = put_uint(p, MILESTONES[k]);
      p = scat(p, " : ");
      p = scat(p, B[b].name);
      scat(p, " x2 !");
      say(s, C_ORANGE, 3000);
      break;
    }
  if (b == NB - 1 && before == 0 && !(S.achieved >> 8 & 1)) {
    S.achieved |= 1u << 8;
    say("Une base sur Mars ! Tu as conquis l'espace !", C_OK, 4000);
  }
  compute_rates();
}
static void buy_bonus(int i) {
  if (U[i].cost > S.money) {
    say("Pas assez d'argent !", C_NO, 1500);
    return;
  }
  S.money -= U[i].cost;
  S.ups |= 1ull << U[i].id;
  char s[64], *p = scat(s, "Acheté : ");
  scat(p, U[i].name);
  say(s, C_OK, 2000);
  compute_rates();
  h_tabs = 1;
}
static void sell_company(void) {
  double g = investors_gain();
  if (g < 1) {
    say("Pas encore d'investisseurs intéressés.", C_NO, 2000);
    return;
  }
  if (now_ms > confirm_until) {
    confirm_until = now_ms + 3000;
    say("Sûr ? Appuie encore sur OK pour vendre.", C_EVENT, 3000);
    return;
  }
  confirm_until = 0;
  S.investors += g;
  S.money = S.run = 0;
  S.ups = 0;
  for (int b = 0; b < NB; b++) S.count[b] = 0;
  sel[0] = sel[1] = scroll[0] = scroll[1] = 0;
  compute_rates();
  save();
  char s[64], a[24], *p = scat(s, "Vendu ! +");
  fmt(g, a);
  p = scat(p, a);
  scat(p, " investisseurs");
  say(s, C_OK, 3500);
  draw_all(true);
}

static void start_event(void) {
  event_digit = 1 + (int)rnd(9);
  event_end = now_ms + 9000;
}
static int digit_of(uint64_t pressed) {
  static const uint8_t K[10] = {eadk_key_zero, eadk_key_one,   eadk_key_two,   eadk_key_three, eadk_key_four,
                                eadk_key_five, eadk_key_six,   eadk_key_seven, eadk_key_eight, eadk_key_nine};
  for (int d = 0; d < 10; d++)
    if (pressed & KEY(K[d])) return d;
  return -1;
}
static void catch_client(void) {
  event_digit = 0;
  next_event = now_ms + 40000 + rnd(50000);
  if (rnd(2)) {
    double base = income / boost;
    double v = base * 60 > click_gain * 40 ? base * 60 : click_gain * 40;
    if (v < 20) v = 20;
    earn(v);
    char s[64], a[24], *p = scat(s, "Client mystère : +");
    fmt(v, a);
    p = scat(p, a);
    scat(p, " $ !");
    say(s, C_OK, 3000);
  } else {
    boost = 5;
    boost_until = now_ms + 20000;
    compute_rates();
    say("RUSH ! Tout rapporte x5 pendant 20 s !", C_ORANGE, 3000);
  }
}

/* ------------------------------------------------------------ écran titre */
static bool splash(bool loaded) {
  fill(0, 0, 320, 240, C_HEAD);
  big_icon(IC_CLICK, 112, 10, 8);
  text("TYCOON", 160 - 3 * LW, 110, true, C_GOLD, C_HEAD);
  text("Construis ton empire !", 160 - 22 * SW / 2, 131, false, C_MINT, C_HEAD);
  static const char *const HELP[] = {"EXE : gagner de l'argent", "OK : acheter     shift : x1 / x10 / MAX",
                                     "Chiffres : attraper les clients mystère",
                                     "Maison ou Retour : sauver et quitter"};
  for (int i = 0; i < 4; i++) text(HELP[i], 20, 152 + i * 15, false, C_WHITE, C_HEAD);
  if (loaded) {
    char s[48], a[24], *p = scat(s, "Partie chargée : ");
    fmt(S.money, a);
    p = scat(p, a);
    scat(p, " $");
    text(s, 160 - ulen(s) * SW / 2, 2, false, C_MINT, C_HEAD);
  }
  uint64_t prev = eadk_keyboard_scan();
  for (int f = 0;; f++) {
    uint64_t k = eadk_keyboard_scan(), pressed = k & ~prev;
    prev = k;
    if (k & (KEY(eadk_key_home) | KEY(eadk_key_on_off) | KEY(eadk_key_back))) return false;
    if (pressed & (KEY(eadk_key_ok) | KEY(eadk_key_exe))) return true;
    if (f % 25 == 0) text("Appuie sur OK pour jouer", 160 - 24 * SW / 2, 220, false, (f / 25) % 2 ? C_HEAD : C_GOLD, C_HEAD);
    eadk_timing_msleep(20);
  }
}

/* ------------------------------------------------------------ boucle principale */
int main(int argc, char *argv[]) {
  np_app_begin();
  init_upgrades();
  bool loaded = load();
  if (!loaded) new_game();
#ifdef HOST
  if (getenv("TYCOON_MONEY")) S.money = S.run = S.total = atof(getenv("TYCOON_MONEY"));
#endif
  now_ms = eadk_timing_millis();
  compute_rates();
  if (!splash(loaded)) {
    if (loaded) save();
    return np_app_end();
  }
  draw_all(true);
  if (!loaded) say("Appuie sur EXE pour gagner tes premiers $ !", C_HEAD2, 5000);
  uint64_t last = eadk_timing_millis(), last_save = last, sec_acc = 0, last_draw = 0;
  uint64_t prev = eadk_keyboard_scan(), hold_start = 0, last_repeat = 0;
  next_event = last + 30000 + rnd(30000);
  for (;;) {
    uint64_t k = eadk_keyboard_scan(), pressed = k & ~prev;
    prev = k;
    now_ms = eadk_timing_millis();
    if (k & (KEY(eadk_key_home) | KEY(eadk_key_on_off))) break;

    /* flèches haut / bas avec répétition */
    uint64_t ud = k & (KEY(eadk_key_up) | KEY(eadk_key_down));
    uint64_t move = pressed & ud;
    if (ud && !move) {
      if (now_ms - hold_start > 350 && now_ms - last_repeat > 90) move = ud, last_repeat = now_ms;
    } else if (move) {
      hold_start = now_ms;
    }

    if (pressed & KEY(eadk_key_back)) {
      if (tab) tab = 0, draw_all(true);
      else break;
    }
    if (pressed & (KEY(eadk_key_left) | KEY(eadk_key_right))) {
      tab = (tab + ((pressed & KEY(eadk_key_right)) ? 1 : 2)) % 3;
      confirm_until = 0;
      draw_all(true);
    }
    if (move && tab < 2) {
      int ids[NU + NB], n = list_rows(ids);
      if ((move & KEY(eadk_key_up)) && sel[tab] > 0) sel[tab]--;
      if ((move & KEY(eadk_key_down)) && sel[tab] < n - 1) sel[tab]++;
    }
    if (pressed & KEY(eadk_key_shift)) S.buymode = (uint8_t)((S.buymode + 1) % 3);
    if (pressed & KEY(eadk_key_exe)) earn(click_gain);
    if (pressed & KEY(eadk_key_ok)) {
      int ids[NU + NB], n = list_rows(ids);
      if (tab == 0 && n && ids[sel[0]] < 100) buy_building(ids[sel[0]]);
      else if (tab == 0 && n) say("Débloqué en achetant le bâtiment d'avant.", C_GREY, 2000);
      else if (tab == 1 && n) buy_bonus(ids[sel[1]]);
      else if (tab == 2) sell_company();
    }
    int d = digit_of(pressed);
    if (event_digit && d >= 0) {
      if (d == event_digit) catch_client();
      else {
        event_digit = 0;
        next_event = now_ms + 40000 + rnd(50000);
        say("Raté ! Le client est parti...", C_NO, 2000);
      }
    }

    /* le temps passe */
    uint64_t dt = now_ms - last;
    last = now_ms;
    if (dt > 5000) dt = 5000;
    if (boost > 1 && now_ms >= boost_until) boost = 1, compute_rates();
    earn(income * (double)dt / 1000.0);
    sec_acc += dt;
    while (sec_acc >= 1000) sec_acc -= 1000, S.playsec++;
    if (event_digit && now_ms >= event_end) {
      event_digit = 0;
      next_event = now_ms + 40000 + rnd(50000);
      say("Le client mystère est parti...", C_GREY, 1500);
    }
    if (!event_digit && S.count[0] > 0 && now_ms >= next_event) start_event();
    check_achievements();
    if (now_ms - last_save >= 20000) save(), last_save = now_ms;

    if (pressed || move || now_ms - last_draw >= 100) {
      draw_all(false);
      last_draw = now_ms;
    }
    eadk_timing_msleep(15);
  }
  save();
  return np_app_end();
}
