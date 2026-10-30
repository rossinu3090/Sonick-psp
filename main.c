/* Sonic PSP fan game - SDL2 / pspdev. All art is placeholder rectangles. */
#include <SDL.h>
#include "tiles.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>

#define W 480
#define H 272
#define GROUND 200
#define PH 22
enum { B_TRI, B_CIR, B_CRO, B_SQU, B_L, B_R, B_DN, B_LF, B_UP, B_RT, B_SEL, B_STA };
enum { S_MENU, S_SEL, S_CTRL, S_PLAY, S_CLEAR, S_END, S_ABOUT };
enum { SPINDASH, AIRBOOST, TELEPORT, SPEAR, HOVER, WAVE, SLAM, WHIP, HAMMER, HAMJUMP, FLY, SWIPE, GLIDE, PUNCH };

typedef struct { const char *name; float spd, jmp; int r, g, b; int ka, kb; const char *na, *nb; } Ch;
static const Ch CH[9] = {
 {"SONIC",   1.0f, 1.0f,  40,  90, 230, SPINDASH, AIRBOOST, "SPIN DASH",     "AIR BOOST"},
 {"SHADOW",  1.0f, 1.0f,  40,  40,  50, TELEPORT, SPEAR,    "CHAOS CONTROL", "CHAOS SPEAR"},
 {"SILVER",  0.95f,1.0f, 190, 205, 215, HOVER,    WAVE,     "PSYCHO HOVER",  "PSYCHO WAVE"},
 {"CHAOS",   0.95f,0.95f, 90, 170, 240, SLAM,     WHIP,     "WATER SLAM",    "TAIL WHIP"},
 {"AMY",     0.9f, 1.05f,240, 110, 170, HAMMER,   HAMJUMP,  "HAMMER SWING",  "HAMMER JUMP"},
 {"TAILS",   0.9f, 1.0f, 245, 170,  40, FLY,      SWIPE,    "TAIL FLIGHT",   "TAIL SWIPE"},
 {"KNUCKLES",1.0f, 0.95f,220,  40,  40, GLIDE,    PUNCH,    "GLIDE",         "PUNCH DASH"},
 {"METAL OVERLORD",1.0f,1.0f,150, 155, 175, HOVER,    SPEAR,    "JET HOVER",     "LASER BLAST"},
 {"METAL SONIC",1.05f,1.0f, 60,  80, 190, AIRBOOST, WAVE,     "ROCKET DASH",   "ELECTRO SHOCK"}};
static const char *ZONE[3] = {"TECHNO BASE", "NEON CITY", "SKY FORTRESS"};
static const char *BOSSNAME[3] = {"EGGMAN", "EGGMAN", "EGGMAN METAL OVERLORD"};

/* ---- tiny 3x5 font ---- */
static const char *CHARS = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789:-./> ";
static const char *GL[42] = {
"010101111101101","110101110101110","011100100100011","110101101101110","111100110100111",
"111100110100100","011100101101011","101101111101101","111010010010111","001001001101010",
"101101110101101","100100100100111","101111111101101","110101101101101","010101101101010",
"110101110100100","010101101111011","110101110101101","011100010001110","111010010010010",
"101101101101111","101101101101010","101101111111101","101101010101101","101101010010010",
"111001010100111",
"111101101101111","010110010010111","111001111100111","111001111001111","101101111001001",
"111100111001111","111100111101111","111001001010010","111101111101111","111101111001111",
"000010000010000","000000111000000","000000000000010","001001010100100","100010001010100",
"000000000000000"};

static SDL_Renderer *R;
static SDL_Joystick *joy;
static int cur[12], prv[12];
static int axis;
#define PRESSED(i) (cur[i] && !prv[i])
enum { SF_JUMP, SF_RING, SF_KILL, SF_HURT, SF_ABIL, SF_EMER, SF_SUPER, SF_BHIT, SF_CLEAR, SF_SHOOT, SF_MENU };
static void sfx(int id);
static int ft; /* frame counter */

static void rect(int x, int y, int w, int h, int r, int g, int b) {
  SDL_SetRenderDrawColor(R, r, g, b, 255);
  SDL_Rect q = {x, y, w, h};
  SDL_RenderFillRect(R, &q);
}
static void text(const char *s, int x, int y, int sc, int r, int g, int b) {
  for (; *s; s++, x += 4 * sc) {
    const char *p = strchr(CHARS, toupper((unsigned char)*s));
    if (!p) continue;
    const char *gl = GL[p - CHARS];
    for (int i = 0; i < 15; i++)
      if (gl[i] == '1') rect(x + (i % 3) * sc, y + (i / 3) * sc, sc, sc, r, g, b);
  }
}
static void ctext(const char *s, int y, int sc, int r, int g, int b) {
  text(s, (W - (int)strlen(s) * 4 * sc) / 2, y, sc, r, g, b);
}

/* ---- tile textures (Stage 1 art, built into tiles.h) ---- */
static SDL_Texture *TT[TILE_N];
static void loadTiles(void) {
  for (int t = 0; t < TILE_N; t++) {
    SDL_Surface *sf = SDL_CreateRGBSurface(0, 96, 96, 32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);
    if (!sf) continue;
    Uint32 *p = (Uint32 *)sf->pixels;
    for (int i = 0; i < 96 * 96; i++) p[i] = TILE_PAL[TILE_DATA[t][i]];
    TT[t] = SDL_CreateTextureFromSurface(R, sf);
    if (TT[t]) SDL_SetTextureBlendMode(TT[t], SDL_BLENDMODE_BLEND);
    SDL_FreeSurface(sf);
  }
}
/* draw part (sx,sy,sw,sh) of tile t at (dx,dy) sized (dw,dh), tinted r,g,b */
static void tile(int t, int sx, int sy, int sw, int sh, int dx, int dy, int dw, int dh, int r, int g, int b) {
  if (!TT[t]) return;
  SDL_Rect a = {sx, sy, sw, sh}, d = {dx, dy, dw, dh};
  SDL_SetTextureColorMod(TT[t], r, g, b);
  SDL_RenderCopy(R, TT[t], &a, &d);
}

/* ---- game state ---- */
static int state = S_MENU, msel, csel, zone, act, cheat, realEm, paused, psel;
static int got[9][2];
#define LV (zone * 3 + act)
#define ISBOSS (act == 2)
static int allEm;
static int EM(void) { return (cheat || allEm) ? 7 : realEm; }

static float LEN;
static int gx[16], gw[16], ng;
static int px[24], py[24], pw[24], np;
static struct { float x, x0, y; int alive, type, dir; } en[40];
static int nen;
static int rx[160], ry[160], rt[160], nr;
static int emx[2], emy[2], nem;
static float sx[6], sy[6], svx[6]; static int slife[6];
static struct { float x, y, vx; int hp, max, inv, t, dive, dt; } B;
static float bsx[8], bsy[8], bsvx[8], bsvy[8]; static int bslife[8];

static struct {
  float x, y, vx, vy; int ground, face, rings, inv, atk, reach, fuel, super, sct, cd[2], who;
} P;

static void addring(int x, int y) { if (nr < 160) { rx[nr] = x; ry[nr] = y; rt[nr] = 0; nr++; } }
static int solid(float x) {
  for (int i = 0; i < ng; i++) if (x > gx[i] && x < gx[i] + gw[i]) return 0;
  return 1;
}
static void gen(int lv) {
  srand(lv * 7919 + 13);
  ng = np = nen = nr = nem = 0;
  memset(bslife, 0, sizeof bslife);
  if (lv % 3 == 2) { /* boss arena */
    LEN = 400; B.x = 300; B.y = GROUND - 80; B.vx = 1.2f + (lv / 3) * 0.5f;
    B.max = B.hp = 6 + (lv / 3) * 3; B.inv = 0; B.t = 0; B.dive = 0; B.dt = 0;
    return;
  }
  LEN = 3600 + lv * 400;
  for (int x = 500; x < LEN - 300; x += 300 + rand() % 150) {
    int t = rand() % 3;
    if (t == 0 && ng < 15) {
      gx[ng] = x; gw[ng] = 50 + rand() % 30; ng++;
      for (int i = 0; i < 5; i++) addring(x - 20 + i * 25, GROUND - 50 - (i % 3 == 1 ? 20 : 0));
    } else if (t == 1 && np < 20) {
      px[np] = x; py[np] = GROUND - 40; pw[np] = 90; np++;
      for (int i = 0; i < 4; i++) addring(x + 10 + i * 22, GROUND - 60);
    } else if (nen < 32) {
      en[nen].x = en[nen].x0 = x; en[nen].alive = 1; en[nen].type = rand() % 3; en[nen].dir = 1; en[nen].y = GROUND - 16; nen++;
      if (nen < 34 && rand() % 2) {
        en[nen].x = en[nen].x0 = x + 120; en[nen].alive = 1; en[nen].type = rand() % 3; en[nen].dir = -1; en[nen].y = GROUND - 16; nen++;
      }
      for (int i = 0; i < 3; i++) addring(x - 30 + i * 30, GROUND - 20);
    }
  }
  px[np] = (int)LEN / 2 - 45; py[np] = GROUND - 40; pw[np] = 90; np++;
  emx[0] = (int)LEN / 2 - 8; emy[0] = GROUND - 62; nem = 1;
  if (lv == 7) {
    px[np] = (int)LEN - 220; py[np] = GROUND - 40; pw[np] = 90; np++;
    emx[1] = (int)LEN - 183; emy[1] = GROUND - 62; nem = 2;
  }
}
static void startAct(void) {
  gen(LV);
  P.x = 60; P.y = GROUND - PH; P.vx = P.vy = 0; P.ground = 1; P.face = 1;
  P.inv = 0; P.atk = 0; P.super = 0; P.sct = 0; P.cd[0] = P.cd[1] = 0; P.fuel = 90;
  P.rings = cheat ? 60 : 0;
  memset(slife, 0, sizeof slife);
}
static void hurt(void) {
  if (P.inv > 0 || P.super) return;
  sfx(SF_HURT);
  if (P.rings > 0) { P.rings = 0; P.inv = 90; P.vy = -3; P.vx = -P.face * 2; }
  else startAct();
}
static int isHold(int k) { return k == HOVER || k == GLIDE || k == FLY; }

static void ability(int k) {
  P.reach = 10;
  sfx(k == SPEAR ? SF_SHOOT : SF_ABIL);
  switch (k) {
    case SPINDASH: P.atk = 25; P.vx = P.face * 7; break;
    case AIRBOOST: P.atk = 20; P.vx = P.face * 6; P.vy = -1; break;
    case TELEPORT: P.x += P.face * 90; P.atk = 10; P.inv = 20; break;
    case SPEAR:
      for (int i = 0; i < 6; i++) if (!slife[i]) {
        sx[i] = P.x; sy[i] = P.y + 6; svx[i] = P.face * 8; slife[i] = 60; break; }
      break;
    case WAVE: P.atk = 15; P.reach = 55; break;
    case SLAM: if (!P.ground) { P.vy = 9; P.atk = 30; P.reach = 25; } break;
    case WHIP: P.atk = 15; P.reach = 30; break;
    case HAMMER: P.atk = 15; P.reach = 28; break;
    case HAMJUMP: P.vy = -8; P.ground = 0; break;
    case SWIPE: P.atk = 15; P.reach = 30; break;
    case PUNCH: P.atk = 18; P.vx = P.face * 6; break;
  }
}

static void bshoot(float vx, float vy, int bh) {
  sfx(SF_SHOOT);
  for (int i = 0; i < 8; i++) if (!bslife[i]) {
    bsx[i] = B.x; bsy[i] = B.y + bh / 2; bsvx[i] = vx; bsvy[i] = vy; bslife[i] = 200; break; }
}
static void bossHit(void) { B.hp--; B.inv = 40; sfx(SF_BHIT); }
static void updateBoss(void) {
  int bw = zone == 2 ? 72 : 56, bh = zone == 2 ? 48 : 36;
  B.t++;
  if (B.inv > 0) B.inv--;
  if (B.dive == 0) {
    B.x += B.vx;
    if (B.x < 60) { B.x = 60; B.vx = fabsf(B.vx); }
    if (B.x > 420) { B.x = 420; B.vx = -fabsf(B.vx); }
    B.y += ((GROUND - 80) - B.y) * 0.05f;
    if (B.t % (110 - zone * 20) == 0) {
      float vx = (P.x - B.x) / 60.0f; if (vx > 3) vx = 3; if (vx < -3) vx = -3;
      bshoot(vx, 2.2f, bh);
      if (zone == 2) { bshoot(vx - 1.5f, 2.2f, bh); bshoot(vx + 1.5f, 2.2f, bh); }
    }
    if (B.t % 260 == 0) B.dive = 1;
  } else if (B.dive == 1) {
    B.y += 2.5f;
    if (B.y >= GROUND - bh) { B.y = GROUND - bh; B.dive = 2; B.dt = 0; }
  } else if (++B.dt > 80) { B.dive = 0; B.dt = 0; }

  for (int i = 0; i < 8; i++) if (bslife[i]) {
    bsx[i] += bsvx[i]; bsy[i] += bsvy[i]; bslife[i]--;
    if (bsy[i] > GROUND) bslife[i] = 0;
    if (fabsf(bsx[i] - (P.x + 7)) < 10 && fabsf(bsy[i] - (P.y + 11)) < 14) { bslife[i] = 0; hurt(); }
  }
  /* player projectiles */
  for (int i = 0; i < 6; i++) if (slife[i] && B.inv == 0 &&
      fabsf(sx[i] - B.x) < bw / 2 && fabsf(sy[i] - (B.y + bh / 2)) < bh / 2) { slife[i] = 0; bossHit(); }
  /* player body */
  int reach = P.atk > 0 ? P.reach : (P.super ? 12 : 0);
  float bx0 = B.x - bw / 2, by0 = B.y;
  int ov = P.x + 14 + reach > bx0 && P.x - reach < bx0 + bw &&
           P.y + PH + reach > by0 && P.y - reach < by0 + bh;
  if (ov && B.inv == 0) {
    if (reach > 0) bossHit();
    else if (P.vy > 0 && P.y + PH < by0 + 16) { bossHit(); P.vy = -4.5f; }
    else hurt();
  }
  if (B.hp <= 0) { state = S_CLEAR; sfx(SF_CLEAR); }
}

static void updatePlay(void) {
  const Ch *c = &CH[P.who];
  if (PRESSED(B_STA)) { paused = !paused; psel = 0; sfx(SF_MENU); }
  if (paused) {
    if (PRESSED(B_UP) || PRESSED(B_DN)) { psel = !psel; sfx(SF_MENU); }
    if (PRESSED(B_CRO)) {
      sfx(SF_MENU);
      if (psel == 0) paused = 0;          /* RESUME */
      else { paused = 0; state = S_MENU; } /* EXIT LEVEL -> main menu */
    }
    return;
  }
  if (PRESSED(B_SEL)) {
    if (P.super) P.super = 0;
    else if (EM() >= 7 && P.rings >= 10) { P.super = 1; P.sct = 0; sfx(SF_SUPER); }
  }
  if (P.super && ++P.sct >= 60) { P.sct = 0; if (--P.rings <= 0) { P.rings = 0; P.super = 0; } }

  float sm = P.super ? (P.who == 8 ? 1.4f : 1.3f) : 1.0f;
  float maxs = 3.2f * c->spd * sm;
  int dir = (cur[B_RT] || axis > 8000) - (cur[B_LF] || axis < -8000);
  float acc = P.ground ? 0.14f : 0.09f;
  if (dir) { P.vx += dir * acc; P.face = dir; }
  else if (P.ground) { float f = 0.1f; if (fabsf(P.vx) < f) P.vx = 0; else P.vx -= (P.vx > 0 ? f : -f); }
  if (fabsf(P.vx) > maxs) P.vx *= (P.atk > 0 ? 0.985f : 0.95f);

  if (PRESSED(B_CRO) && P.ground) { P.vy = -5.0f * c->jmp * (P.super ? 1.12f : 1.0f); P.ground = 0; sfx(SF_JUMP); }
  if (P.vy < -2 && !cur[B_CRO] && P.atk == 0) P.vy = -2;
  P.vy += 0.25f;

  /* abilities: L = slot 0, R = slot 1 */
  int btn[2] = {B_L, B_R}, kind[2] = {c->ka, c->kb};
  for (int i = 0; i < 2; i++) {
    if (P.cd[i] > 0) P.cd[i]--;
    if (isHold(kind[i])) {
      if (cur[btn[i]]) {
        if (kind[i] == HOVER && P.vy > 0.5f) P.vy = 0.5f;
        if (kind[i] == GLIDE && !P.ground && P.vy > 0) { P.vy = 0.6f; P.vx = P.face * 3.5f * sm; }
        if (kind[i] == FLY && P.fuel > 0) { P.fuel--; P.vy -= 0.45f; if (P.vy < -2.5f) P.vy = -2.5f; }
      }
    } else if (PRESSED(btn[i]) && P.cd[i] == 0) { ability(kind[i]); P.cd[i] = 30; }
  }
  if (P.vy > 10) P.vy = 10;
  if (P.atk > 0) P.atk--;
  if (P.inv > 0) P.inv--;

  P.x += P.vx;
  if (P.x < 0) { P.x = 0; P.vx = 0; }
  if (P.x > LEN + 100) P.x = LEN + 100;
  if (ISBOSS && P.x > W - 14) P.x = W - 14;
  float pf = P.y + PH;
  P.y += P.vy;
  float f = P.y + PH;
  P.ground = 0;
  if (P.vy >= 0) {
    if (f >= GROUND && pf <= GROUND + 1 && solid(P.x + 7)) { P.y = GROUND - PH; P.vy = 0; P.ground = 1; }
    for (int i = 0; i < np; i++)
      if (pf <= py[i] + 1 && f >= py[i] && P.x + 14 > px[i] && P.x < px[i] + pw[i]) {
        P.y = py[i] - PH; P.vy = 0; P.ground = 1; }
  }
  if (P.ground) P.fuel = 90;
  if (P.y > H + 40) startAct();

  /* projectiles */
  for (int i = 0; i < 6; i++) if (slife[i]) {
    sx[i] += svx[i]; slife[i]--;
    for (int e = 0; e < nen; e++)
      if (en[e].alive && fabsf(en[e].x - sx[i]) < 14 && fabsf((en[e].y + 8) - sy[i]) < 20) { en[e].alive = 0; slife[i] = 0; sfx(SF_KILL); }
  }
  /* enemies: 0 = crawler, 1 = buzzer (flies), 2 = spiker (do not stomp!) */
  ft++;
  for (int e = 0; e < nen; e++) if (en[e].alive) {
    int ty = en[e].type, eh = ty == 1 ? 12 : 16;
    float spd = ty == 1 ? 0.9f : (ty == 2 ? 0.4f : 0.6f), rng = ty == 1 ? 60 : 40;
    en[e].x += spd * en[e].dir;
    if (fabsf(en[e].x - en[e].x0) > rng) en[e].dir = -en[e].dir;
    en[e].y = ty == 1 ? GROUND - 62 + 14 * sinf(ft * 0.06f + e) : GROUND - eh;
    float ey = en[e].y;
    int reach = P.atk > 0 ? P.reach : (P.super ? 12 : 0);
    int hit = P.x + 14 + reach > en[e].x && P.x - reach < en[e].x + 16 &&
              P.y + PH + (P.atk > 0 ? reach : 0) > ey && P.y - (P.atk > 0 ? reach : 0) < ey + eh;
    if (!hit) continue;
    if (reach > 0) { en[e].alive = 0; sfx(SF_KILL); }
    else if (ty != 2 && P.vy > 0 && P.y + PH < ey + 10) { en[e].alive = 0; P.vy = -3.5f; sfx(SF_KILL); }
    else hurt();
  }
  /* rings & emeralds */
  for (int i = 0; i < nr; i++)
    if (!rt[i] && fabsf(rx[i] - (P.x + 7)) < 14 && fabsf(ry[i] - (P.y + 11)) < 16) { rt[i] = 1; P.rings++; sfx(SF_RING); }
  for (int i = 0; i < nem; i++) {
    int lv = LV;
    if (!got[lv][i] && fabsf(emx[i] + 8 - (P.x + 7)) < 14 && fabsf(emy[i] - P.y) < 24) {
      got[lv][i] = 1; if (realEm < 7) realEm++; sfx(SF_EMER); }
  }
  if (ISBOSS) updateBoss(); else if (P.x >= LEN) { state = S_CLEAR; sfx(SF_CLEAR); }
}

/* ---- drawing ---- */
static void drawChar(int who, int x, int y, int face, int super, int sc) {
  const Ch *c = &CH[who];
  int neo = super && who == 8; /* Metal Sonic's super form = Neo Metal Sonic */
  int r = neo ? 120 : (super ? 255 : c->r), g = neo ? 40 : (super ? 215 : c->g), b = neo ? 200 : (super ? 40 : c->b);
  if (neo) { /* red-orange aura + big spiky quills */
    rect(x - 2 * sc, y + 6 * sc, 18 * sc, 18 * sc, 120, 20, 40);
    rect(x + (face > 0 ? -5 : 12) * sc, y - 2 * sc, 7 * sc, 3 * sc, 255, 90, 30);
    rect(x + (face > 0 ? -4 : 11) * sc, y + 3 * sc, 6 * sc, 3 * sc, 255, 90, 30);
  }
  rect(x, y + 8 * sc, 14 * sc, 14 * sc, r, g, b);
  rect(x + (face > 0 ? 2 : 0) * sc, y, 12 * sc, 10 * sc, r, g, b);
  rect(x + (face > 0 ? 8 : 2) * sc, y + 3 * sc, 4 * sc, 4 * sc, neo ? 255 : 255, neo ? 60 : 255, neo ? 60 : 255);
  rect(x + (face > 0 ? 10 : 2) * sc, y + 4 * sc, 2 * sc, 2 * sc, 0, 0, 0);
  rect(x + (face > 0 ? -2 : 12) * sc, y + 2 * sc, 4 * sc, 3 * sc, r * 3 / 4, g * 3 / 4, b * 3 / 4);
}
static void drawBoss(float cam) {
  for (int i = 0; i < 8; i++) if (bslife[i]) rect((int)(bsx[i] - cam) - 4, (int)bsy[i] - 4, 8, 8, 255, 140, 0);
  if (B.inv > 0 && (B.inv / 3) % 2) return;
  int x = (int)(B.x - cam), y = (int)B.y;
  if (zone < 2) {
    rect(x - 28, y + 16, 56, 20, zone ? 90 : 200, zone ? 90 : 60, zone ? 140 : 60);
    rect(x - 16, y, 32, 18, 200, 230, 255);
    rect(x - 8, y + 4, 16, 12, 240, 190, 150);
    rect(x - 6, y + 11, 12, 4, 200, 60, 30);
    rect(x - 34, y + 22, 6, 8, 80, 80, 80); rect(x + 28, y + 22, 6, 8, 80, 80, 80);
  } else {
    rect(x - 36, y, 72, 48, 140, 145, 165);
    rect(x - 24, y + 8, 48, 16, 30, 30, 40);
    rect(x - 16, y + 12, 10, 8, 255, 40, 40); rect(x + 6, y + 12, 10, 8, 255, 40, 40);
    rect(x - 46, y + 14, 10, 26, 100, 105, 125); rect(x + 36, y + 14, 10, 26, 100, 105, 125);
    rect(x - 10, y - 8, 20, 10, 240, 190, 150);
  }
}
static void drawPlay(void) {
  static const int sky[3][3] = {{110,180,250},{30,20,70},{240,150,120}};
  static const int gr[3][3] = {{60,170,60},{70,70,110},{150,110,90}};
  const int *s = sky[zone], *g = gr[zone];
  SDL_SetRenderDrawColor(R, s[0], s[1], s[2], 255); SDL_RenderClear(R);
  float cam = P.x - 200; if (cam < 0) cam = 0; if (cam > LEN + 100 - W) cam = LEN + 100 - W;
  if (zone == 0) {
    /* Stage 1 background from the tile rip: dark sky, far girders, ceiling girders */
    rect(0, 0, W, GROUND, 18, 26, 54);
    rect(0, 90, W, 60, 24, 34, 70);
    int base = (int)(cam * 0.25f), first = base / 48;
    for (int k = -1; k < W / 48 + 2; k++) {            /* far layer: tinted dark */
      int n = first + k, dx = n * 48 - base;
      tile(2 + (n & 1), 0, 0, 96, 96, dx, 70, 48, 48, 60, 70, 110);
    }
    base = (int)(cam * 0.5f); first = base / 48;
    for (int k = -1; k < W / 48 + 2; k++) {            /* near layer: ceiling girders */
      int n = first + k, dx = n * 48 - base;
      tile(n & 1, 0, 0, 96, 96, dx, 0, 48, 48, 150, 160, 200);
    }
  } else
  for (int x = 0; x < W; x += 8) {
    int h = 30 + (int)(25 * sinf((x + cam * 0.3f) / 40.0f));
    rect(x, GROUND - h, 8, h, s[0] * 2 / 3, s[1] * 2 / 3 + 20, s[2] * 2 / 3);
  }
  for (int x = 0; x < W; x += 16) {
    float wx = cam + x; wx -= fmodf(wx, 16);
    if (solid(wx + 8)) {
      int k = ((int)wx / 16) & 1, dx = x - (int)fmodf(cam, 16);
      if (zone == 0) {   /* steel floor: girder tile on top, dark metal below */
        rect(dx, GROUND, 16, H - GROUND, 34 - k * 6, 40 - k * 6, 70 - k * 8);
        tile(2 + (((int)wx / 96) & 1), ((int)wx % 48) * 2, 0, 32, 48, dx, GROUND - 3, 16, 24, 255, 255, 255);
      } else
      rect(dx, GROUND, 16, H - GROUND, g[0] - k * 20, g[1] - k * 20, g[2] - k * 20);
    }
  }
  for (int i = 0; i < np; i++) {
    if (zone == 0) {  /* floating girder platforms */
      for (int xx = 0; xx < pw[i]; xx += 16) {
        int w = pw[i] - xx < 16 ? pw[i] - xx : 16;
        tile(3, (xx % 48) * 2, 0, w * 2, 44, px[i] + xx - (int)cam, py[i] - 3, w, 22, 255, 255, 255);
      }
    } else rect(px[i] - (int)cam, py[i], pw[i], 8, g[0], g[1], g[2]);
  }
  for (int i = 0; i < nr; i++) if (!rt[i]) rect(rx[i] - (int)cam, ry[i], 8, 8, 255, 220, 0);
  for (int i = 0; i < nem; i++) if (!got[LV][i]) rect(emx[i] - (int)cam, emy[i], 14, 14, 60, 255, 120);
  for (int e = 0; e < nen; e++) if (en[e].alive) {
    int ex = (int)en[e].x - (int)cam, ey = (int)en[e].y;
    if (en[e].type == 0) {            /* crawler: red bug */
      rect(ex, ey + 4, 16, 12, 200, 30, 30); rect(ex + 3, ey, 10, 6, 230, 70, 60);
      rect(ex + 9, ey + 5, 3, 3, 255, 255, 255);
    } else if (en[e].type == 1) {     /* buzzer: yellow flyer with wings */
      rect(ex + 2, ey + 3, 12, 9, 240, 190, 30); rect(ex + 9, ey + 5, 3, 3, 0, 0, 0);
      rect(ex + 2, ey - 2 + (ft / 4) % 2 * 3, 12, 3, 220, 240, 255);
    } else {                          /* spiker: grey with spikes */
      rect(ex, ey + 6, 16, 10, 110, 110, 125);
      for (int k = 0; k < 4; k++) rect(ex + k * 4, ey + 1, 3, 6, 230, 230, 240);
      rect(ex + 9, ey + 9, 3, 3, 255, 60, 60);
    }
  }
  for (int i = 0; i < 6; i++) if (slife[i]) rect((int)(sx[i] - cam), (int)sy[i], 10, 4, 255, 60, 60);
  if (!ISBOSS) {
    rect((int)(LEN - cam), GROUND - 60, 6, 60, 240, 240, 240);
    rect((int)(LEN - cam) + 6, GROUND - 60, 20, 12, 255, 40, 40);
  } else drawBoss(cam);
  if (!(P.inv > 0 && (P.inv / 4) % 2))
    drawChar(P.who, (int)(P.x - cam), (int)P.y, P.face, P.super, 1);
  if (P.atk > 0) rect((int)(P.x - cam) - P.reach, (int)P.y + 8, 14 + 2 * P.reach, 2, 255, 255, 255);

  char b[64];
  snprintf(b, sizeof b, "RINGS %d  EMERALDS %d/7", P.rings, EM());
  text(b, 6, 6, 2, 255, 255, 255);
  if (ISBOSS) snprintf(b, sizeof b, "%s BOSS", ZONE[zone]);
  else snprintf(b, sizeof b, "%s %d-%d", ZONE[zone], zone + 1, act + 1);
  text(b, 6, 22, 2, 255, 255, 255);
  if (ISBOSS) {
    text(BOSSNAME[zone], 250, 234, 1, 255, 255, 255);
    rect(250, 244, 222, 10, 255, 255, 255);
    rect(252, 246, 218, 6, 60, 0, 0);
    rect(252, 246, 218 * (B.hp > 0 ? B.hp : 0) / B.max, 6, 255, 50, 50);
  }
  const Ch *c = &CH[P.who];
  snprintf(b, sizeof b, "L %s%s", c->na, P.cd[0] ? " ..." : "");
  text(b, 6, 252, 1, 255, 255, 255);
  snprintf(b, sizeof b, "R %s%s", c->nb, P.cd[1] ? " ..." : "");
  text(b, 6, 260, 1, 255, 255, 255);
  if (P.super && P.who == 8) text("NEO METAL SONIC", 350, 6, 2, 255, 90, 30);
  else if (P.super) text("SUPER", 420, 6, 2, 255, 215, 0);
  else if (EM() >= 7) text("SELECT: SUPER", 330, 6, 1, 255, 215, 0);
  if (paused) {
    rect(130, 70, 220, 130, 255, 255, 255);
    rect(134, 74, 212, 122, 20, 25, 60);
    ctext("PAUSED", 84, 3, 255, 215, 0);
    ctext(psel == 0 ? "> RESUME" : "  RESUME", 130, 2, psel == 0 ? 255 : 150, psel == 0 ? 255 : 150, psel == 0 ? 80 : 150);
    ctext(psel == 1 ? "> EXIT LEVEL" : "  EXIT LEVEL", 156, 2, psel == 1 ? 255 : 150, psel == 1 ? 255 : 150, psel == 1 ? 80 : 150);
  }
}
static void drawMenu(void) {
  SDL_SetRenderDrawColor(R, 20, 30, 90, 255); SDL_RenderClear(R);
  ctext("SONIC PSP", 40, 6, 255, 215, 0);
  ctext("FAN GAME", 80, 3, 255, 255, 255);
  const char *it[4] = {"START GAME", cheat ? "CHEAT: ON  7 EMERALDS" : "CHEAT: OFF",
    allEm ? "ALL EMERALDS: GIVEN" : "GET ALL EMERALDS", "ABOUT"};
  for (int i = 0; i < 4; i++) {
    char b[40]; snprintf(b, sizeof b, "%s%s", i == msel ? "> " : "  ", it[i]);
    ctext(b, 135 + i * 28, 2, i == msel ? 255 : 150, i == msel ? 255 : 150, i == msel ? 80 : 150);
  }
  ctext("X: SELECT", 242, 1, 200, 200, 200);
  ctext("STAGE 1 TILES: SEGA / DIMPS / TECHOKAMI RIP", 256, 1, 140, 150, 190);
}
static void drawSelect(void) {
  SDL_SetRenderDrawColor(R, 15, 15, 40, 255); SDL_RenderClear(R);
  char b[48]; if (ISBOSS) snprintf(b, sizeof b, "%s BOSS", ZONE[zone]); else snprintf(b, sizeof b, "%s ACT %d", ZONE[zone], act + 1);
  ctext(b, 10, 2, 255, 255, 255);
  ctext("CHOOSE YOUR CHARACTER", 30, 2, 255, 215, 0);
  drawChar(csel, 200, 70, 1, EM() >= 7, 3);
  ctext(CH[csel].name, 150, 3, 255, 255, 255);
  snprintf(b, sizeof b, "L: %s", CH[csel].na); ctext(b, 180, 2, 200, 220, 255);
  snprintf(b, sizeof b, "R: %s", CH[csel].nb); ctext(b, 196, 2, 200, 220, 255);
  for (int i = 0; i < 9; i++) {
    int x = 6 + i * 52, y = i == csel ? 216 : 228;
    rect(x, y, 48, 40, i == csel ? 255 : 60, i == csel ? 255 : 60, i == csel ? 120 : 80);
    rect(x + 3, y + 3, 42, 34, CH[i].r, CH[i].g, CH[i].b);
  }
}
static void drawCtrl(void) {
  SDL_SetRenderDrawColor(R, 10, 40, 40, 255); SDL_RenderClear(R);
  ctext("CONTROLS", 14, 3, 255, 215, 0);
  char b[64]; const char *l[8]; int n = 0; static char la[40], lb[40];
  snprintf(la, sizeof la, "L: %s", CH[csel].na); snprintf(lb, sizeof lb, "R: %s", CH[csel].nb);
  l[n++] = "D-PAD / STICK: MOVE"; l[n++] = "X: JUMP (HOLD FOR HIGHER)";
  l[n++] = la; l[n++] = lb;
  l[n++] = "SELECT: SUPER FORM (7 EMERALDS + 10 RINGS)";
  l[n++] = "START: PAUSE"; l[n++] = ISBOSS ? "GOAL: DEFEAT THE BOSS" : "GOAL: REACH THE RED FLAG";
  for (int i = 0; i < n; i++) text(l[i], 30, 60 + i * 20, 2, 255, 255, 255);
  snprintf(b, sizeof b, "PLAYING AS %s", CH[csel].name);
  ctext(b, 215, 2, 255, 255, 120);
  ctext("PRESS X TO START   O: BACK", 245, 1, 200, 200, 200);
}
static void drawAbout(void) {
  SDL_SetRenderDrawColor(R, 10, 20, 50, 255); SDL_RenderClear(R);
  ctext("ABOUT", 16, 4, 255, 215, 0);
  static const char *l[] = {
    "THIS IS A FREE FAN MADE GAME.",
    "IT USES SETS OF SPRITES FROM",
    "SONIC ADVANCE, TAKEN FROM",
    "SPRITERS-RESOURCE.COM",
    "",
    "THE SPRITES AND THE SONIC",
    "CHARACTERS ARE THE WORK OF SEGA.",
    "",
    "TILE CREDITS: SEGA, DIMPS AND",
    "TECHOKAMI (LEVEL RIP)."};
  for (int i = 0; i < 10; i++) ctext(l[i], 62 + i * 16, 2, 255, 255, 255);
  ctext("PRESS O OR X TO GO BACK", 248, 1, 200, 200, 200);
}
static void drawClear(void) {
  SDL_SetRenderDrawColor(R, 20, 20, 60, 255); SDL_RenderClear(R);
  ctext(ISBOSS ? "BOSS DEFEATED" : "ACT CLEAR", 80, 5, 255, 215, 0);
  char b[40]; snprintf(b, sizeof b, "RINGS %d   EMERALDS %d/7", P.rings, EM());
  ctext(b, 140, 2, 255, 255, 255);
  ctext("PRESS X", 200, 2, 200, 200, 200);
}

/* ---- chiptune soundtrack: one song per zone, boss theme, menu theme ---- */
typedef struct { const int *lead, *bass; int ms; } Song;
static const int L0[16] = {72,0,76,79,0,76,72,0,74,77,0,81,79,0,76,0};
static const int B0[16] = {48,0,48,0,53,0,53,0,55,0,55,0,48,0,43,0};
static const int L1[16] = {69,0,72,76,0,72,69,0,67,0,71,74,0,71,67,0};
static const int B1[16] = {45,0,45,0,41,0,41,0,43,0,43,0,45,0,40,0};
static const int L2[16] = {74,77,81,77,74,77,82,77,72,76,79,76,72,76,81,76};
static const int B2[16] = {38,0,38,38,34,0,34,34,36,0,36,36,33,0,33,33};
static const int L3[16] = {64,0,64,65,0,64,0,63,62,0,62,63,0,62,0,59};
static const int B3[16] = {40,40,0,40,40,40,0,41,38,38,0,38,38,38,0,39};
static const int L4[16] = {67,0,71,74,0,71,67,0,69,0,72,76,0,72,69,0};
static const int B4[16] = {43,0,0,0,47,0,0,0,45,0,0,0,40,0,0,0};
static const Song SONGS[5] = {{L0,B0,150},{L1,B1,130},{L2,B2,110},{L3,B3,90},{L4,B4,200}};
static int aCh = 1, aRate = 22050;
static volatile int track = 4;
static SDL_AudioDeviceID adev;
typedef struct { float f0, f1, ph; int len, pos, noise; } Voice;
static Voice V[4];
static const struct { float f0, f1; int ms, noise; } SFXT[11] = {
  {300, 700, 120, 0},   /* jump */
  {1200, 1800, 90, 0},  /* ring */
  {600, 120, 160, 1},   /* enemy kill */
  {500, 80, 320, 1},    /* hurt */
  {200, 900, 160, 0},   /* ability */
  {600, 1600, 350, 0},  /* emerald */
  {300, 1300, 450, 0},  /* super */
  {200, 60, 220, 1},    /* boss hit */
  {500, 1100, 600, 0},  /* clear */
  {900, 300, 120, 0},   /* shot */
  {800, 800, 50, 0}};   /* menu */
static void sfx(int id) {
  if (!adev) return;
  SDL_LockAudioDevice(adev);
  int slot = 0;
  for (int i = 0; i < 4; i++) if (V[i].pos >= V[i].len) { slot = i; break; }
  V[slot].f0 = SFXT[id].f0; V[slot].f1 = SFXT[id].f1; V[slot].ph = 0; V[slot].pos = 0;
  V[slot].len = SFXT[id].ms * aRate / 1000; V[slot].noise = SFXT[id].noise;
  SDL_UnlockAudioDevice(adev);
}
static void audioCb(void *u, Uint8 *st, int len) {
  (void)u;
  Sint16 *o = (Sint16 *)st;
  int frames = len / (2 * aCh);
  static unsigned pos; static int lastT = -1, lastS = -1; static float f1, f2, p1, p2;
  int t = track;
  if (t != lastT) { lastT = t; pos = 0; lastS = -1; }
  const Song *s = &SONGS[t];
  unsigned sl = aRate * s->ms / 1000;
  for (int i = 0; i < frames; i++) {
    int step = (pos / sl) % 16; unsigned in = pos % sl;
    if (step != lastS) {
      lastS = step; int a = s->lead[step], b = s->bass[step];
      f1 = a ? 440.0f * powf(2.0f, (a - 69) / 12.0f) : 0;
      f2 = b ? 440.0f * powf(2.0f, (b - 69) / 12.0f) : 0;
    }
    float env = 1.0f - 0.7f * in / sl, v = 0;
    if (f1 > 0) { p1 += f1 / aRate; if (p1 >= 1) p1 -= 1; v += (p1 < 0.5f ? 0.11f : -0.11f) * env; }
    if (f2 > 0) { p2 += f2 / aRate; if (p2 >= 1) p2 -= 1; v += (p2 < 0.25f ? 0.09f : -0.09f); }
    for (int k = 0; k < 4; k++) if (V[k].pos < V[k].len) {
      float tt = (float)V[k].pos / V[k].len, f = V[k].f0 + (V[k].f1 - V[k].f0) * tt;
      V[k].ph += f / aRate; if (V[k].ph >= 1) V[k].ph -= 1;
      float w = V[k].noise ? (rand() % 2000 - 1000) / 1000.0f : (V[k].ph < 0.5f ? 1.0f : -1.0f);
      v += w * 0.16f * (1.0f - tt);
      V[k].pos++;
    }
    Sint16 sm = (Sint16)(v * 32000);
    for (int c = 0; c < aCh; c++) *o++ = sm;
    pos++;
  }
}

static void readInput(void) {
  memcpy(prv, cur, sizeof cur);
  SDL_PumpEvents();
  for (int i = 0; i < 12; i++) cur[i] = joy ? SDL_JoystickGetButton(joy, i) : 0;
  axis = joy ? SDL_JoystickGetAxis(joy, 0) : 0;
  const Uint8 *k = SDL_GetKeyboardState(NULL); /* PC test keys */
  cur[B_CRO] |= k[SDL_SCANCODE_Z]; cur[B_CIR] |= k[SDL_SCANCODE_X];
  cur[B_L] |= k[SDL_SCANCODE_A]; cur[B_R] |= k[SDL_SCANCODE_S];
  cur[B_LF] |= k[SDL_SCANCODE_LEFT]; cur[B_RT] |= k[SDL_SCANCODE_RIGHT];
  cur[B_UP] |= k[SDL_SCANCODE_UP]; cur[B_DN] |= k[SDL_SCANCODE_DOWN];
  cur[B_STA] |= k[SDL_SCANCODE_RETURN]; cur[B_SEL] |= k[SDL_SCANCODE_RSHIFT];
}

int main(int argc, char *argv[]) {
  (void)argc; (void)argv;
  SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK | SDL_INIT_AUDIO);
  SDL_Window *win = SDL_CreateWindow("Sonic PSP", 0, 0, W, H, 0);
  R = SDL_CreateRenderer(win, -1, 0);
  loadTiles();
  if (SDL_NumJoysticks() > 0) joy = SDL_JoystickOpen(0);
  SDL_AudioSpec want, have; SDL_zero(want);
  want.freq = 22050; want.format = AUDIO_S16SYS; want.channels = 1; want.samples = 1024; want.callback = audioCb;
  SDL_AudioDeviceID ad = SDL_OpenAudioDevice(NULL, 0, &want, &have,
                         SDL_AUDIO_ALLOW_FREQUENCY_CHANGE | SDL_AUDIO_ALLOW_CHANNELS_CHANGE);
  if (ad) { aCh = have.channels; aRate = have.freq; adev = ad; SDL_PauseAudioDevice(ad, 0); }
  int run = 1;
  while (run) {
    Uint32 t0 = SDL_GetTicks();
    SDL_Event ev; while (SDL_PollEvent(&ev)) if (ev.type == SDL_QUIT) run = 0;
    readInput();
    track = (state == S_PLAY || state == S_CLEAR) ? (ISBOSS ? 3 : zone) : 4;
    switch (state) {
      case S_MENU:
        if (PRESSED(B_UP) && msel > 0) { msel--; sfx(SF_MENU); }
        if (PRESSED(B_DN) && msel < 3) { msel++; sfx(SF_MENU); }
        if (PRESSED(B_CRO)) {
          if (msel == 0) { zone = act = 0; realEm = 0; memset(got, 0, sizeof got); state = S_SEL; }
          else if (msel == 1) { cheat = !cheat; sfx(SF_EMER); }
          else if (msel == 2) { allEm = 1; sfx(SF_EMER); }
          else { state = S_ABOUT; sfx(SF_MENU); }
        }
        drawMenu(); break;
      case S_SEL:
        if (PRESSED(B_LF)) { csel = (csel + 8) % 9; sfx(SF_MENU); }
        if (PRESSED(B_RT)) { csel = (csel + 1) % 9; sfx(SF_MENU); }
        if (PRESSED(B_CRO)) state = S_CTRL;
        if (PRESSED(B_CIR)) state = S_MENU;
        drawSelect(); break;
      case S_CTRL:
        if (PRESSED(B_CIR)) state = S_SEL;
        if (PRESSED(B_CRO)) { sfx(SF_SUPER); P.who = csel; paused = 0; startAct(); state = S_PLAY; }
        drawCtrl(); break;
      case S_ABOUT:
        if (PRESSED(B_CIR) || PRESSED(B_CRO)) { state = S_MENU; sfx(SF_MENU); }
        drawAbout(); break;
      case S_PLAY: updatePlay(); drawPlay(); break;
      case S_CLEAR:
        if (PRESSED(B_CRO)) {
          if (++act > 2) { act = 0; zone++; }
          state = zone > 2 ? S_END : S_SEL;
        }
        drawClear(); break;
      case S_END:
        SDL_SetRenderDrawColor(R, 0, 0, 0, 255); SDL_RenderClear(R);
        ctext("YOU WIN!", 100, 6, 255, 215, 0);
        ctext("PRESS X", 180, 2, 200, 200, 200);
        if (PRESSED(B_CRO)) state = S_MENU;
        break;
    }
    SDL_RenderPresent(R);
    Uint32 dt = SDL_GetTicks() - t0;
    if (dt < 16) SDL_Delay(16 - dt);
  }
  SDL_Quit();
  return 0;
}
