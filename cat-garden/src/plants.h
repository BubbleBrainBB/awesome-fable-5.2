// plants.h — materials and procedural generators for plants / small ground objects
#pragma once
#include "geom.h"

struct Mats {
  int ground, grass, grassTall, seed, clover, cloverFl, dandLeaf, stemGreen, stemPale, petYellow, pappus, petWhite, discYellow, discDark;
  int stone, stoneWet, stoneMoss, litter, litter2, twig, bark, barkBirch, barkDark, mulch;
  int hosta, hostaVar, fern, sprig, sprigDark, treeLeaf, birchLeaf;
  int petPoppy, poppyPod, petFox, foxLeaf, petAllium, lavLeaf, petLav, petPink, petPurple, petOrange, petCosmos;
  int lilyPad, petLily, reed, cattail, irisLeaf, petIris;
  int wood, woodBench, woodPaint, brick, brickUnit, iron, glass, water, paving, terracotta, whiteMetal, galv;
  int banana, monstera, alocasia, calathea, bromeliad, orchidLeaf, petOrchid, petOrchidLip, sarra, sarraHood, flytrap, moss;
  int cactus, cactusCol, spine, opuntia, agave, echeveria, aloe, lithops, cactusFl, sandRock;
  int ladyShell, ladyBlack, beeBody, wingClear, bfWing, bfWing2, bfBody, dfBody, dfEye, koi, koiFin, fur, furPaw, denim, shoe, shoeSole, antBody, whisker, label, labelStake, dew, hose;
  int mushCap, mushStem, snailShell, snailBody, robin, wallStone;
};
static Mats MT;

static inline void makeMats(Scene& S) {
  auto A = [&](int type, V3 a, float rough, V3 b, float trans, V3 c, float p0 = 0, float p1 = 0, float p2 = 0, float p3 = 0, float q0 = 0, float q1 = 0, float sh = 0, float q3 = 0) {
    int id = S.addMat(type, a, rough, b, trans, c, p0, p1, p2, p3, q0, q1, sh, q3);
    if ((type == 1 && (q0 > 0 || q3 > 0 || p2 > 2.5f)) || type == 12) return (int)(id | MAT_ALPHA);
    return id;
  };
  V3 z(0, 0, 0);
  MT.ground = A(6, V3(0.3f, 0.28f, 0.25f), 0.95f, z, 0, z);
  // type 2 grass: a,b greens; c base colour
  MT.grass = A(2, V3(0.040f, 0.125f, 0.016f), 0.42f, V3(0.105f, 0.185f, 0.028f), 0.55f, V3(0.33f, 0.33f, 0.15f), 0, 0, 0, 0, 0, 0, 0.30f);
  MT.grassTall = A(2, V3(0.055f, 0.14f, 0.022f), 0.45f, V3(0.17f, 0.22f, 0.05f), 0.55f, V3(0.36f, 0.34f, 0.17f), 0, 0, 0, 0, 0, 0, 0.30f);
  MT.seed = A(0, V3(0.42f, 0.34f, 0.17f), 0.8f, V3(0.28f, 0.22f, 0.10f), 0.35f, z, 0.6f, 300, 0.3f, 0, 0, 1, 0.3f);
  // type 1 leaves: A(1, colA, rough, colB, trans, veinCol, nVeins, slope, style, varieg, holes, browning, shadowT, serration)
  MT.clover = A(1, V3(0.045f, 0.15f, 0.025f), 0.5f, V3(0.08f, 0.2f, 0.04f), 0.5f, V3(0.2f, 0.32f, 0.12f), 9, 4, 0, 0.35f, 0, 0, 0.3f);
  MT.cloverFl = A(3, V3(0.75f, 0.78f, 0.6f), 0.7f, V3(0.85f, 0.83f, 0.8f), 0.4f, V3(0.8f, 0.6f, 0.65f), 0, 0.5f, 0.6f, 0, 0, 0, 0.4f);
  MT.dandLeaf = A(1, V3(0.045f, 0.14f, 0.02f), 0.5f, V3(0.09f, 0.19f, 0.03f), 0.5f, V3(0.25f, 0.33f, 0.14f), 7, 3, 0, 0, 0.05f, 0.25f, 0.3f);
  MT.stemGreen = A(0, V3(0.07f, 0.17f, 0.035f), 0.5f, V3(0.12f, 0.2f, 0.05f), 0.25f, z, 0.6f, 60, 0.6f, 0, 0, 1, 0.15f);
  MT.stemPale = A(0, V3(0.22f, 0.26f, 0.10f), 0.45f, V3(0.30f, 0.18f, 0.13f), 0.3f, z, 0.7f, 40, 0.7f, 0, 0, 1, 0.2f);
  MT.petYellow = A(3, V3(0.85f, 0.50f, 0.02f), 0.55f, V3(0.92f, 0.68f, 0.02f), 0.45f, V3(0.95f, 0.8f, 0.1f), 0, 0.6f, 0.6f, 0, 0, 0, 0.4f);
  MT.pappus = A(0, V3(0.85f, 0.85f, 0.82f), 0.6f, V3(0.8f, 0.8f, 0.78f), 0.8f, z, 0.1f, 50, 0.3f, 0, 0, 1, 0.75f);
  MT.petWhite = A(3, V3(0.80f, 0.82f, 0.70f), 0.6f, V3(0.88f, 0.88f, 0.86f), 0.45f, V3(0.9f, 0.75f, 0.8f), 0, 0.4f, 0.25f, 0, 0, 0, 0.45f);
  MT.discYellow = A(0, V3(0.80f, 0.52f, 0.03f), 0.8f, V3(0.55f, 0.30f, 0.02f), 0, z, 0.9f, 900, 0.3f);
  MT.discDark = A(0, V3(0.10f, 0.05f, 0.025f), 0.8f, V3(0.30f, 0.14f, 0.03f), 0, z, 0.9f, 700, 0.3f);
  // stones: type 5: p0 = noise freq, p1 = wetness, p2 = fixed colour flag, p3 = moss
  MT.stone = A(5, V3(0.4f, 0.38f, 0.35f), 0.8f, z, 0, z, 1.6f, 0, 0, 0.0f);
  MT.stoneWet = A(5, V3(0.4f, 0.38f, 0.35f), 0.8f, z, 0, z, 1.6f, 0.0f, 0, 1.0f);
  MT.stoneMoss = A(5, V3(0.30f, 0.29f, 0.27f), 0.8f, z, 0, z, 1.2f, 0, 1, 0.8f);
  MT.wallStone = A(5, V3(0.36f, 0.35f, 0.33f), 0.85f, z, 0, z, 1.3f, 0, 0, 0.3f, 1.0f);
  MT.sandRock = A(5, V3(0.50f, 0.36f, 0.24f), 0.85f, z, 0, z, 1.4f, 0, 1, 0.0f);
  MT.litter = A(1, V3(0.20f, 0.11f, 0.04f), 0.75f, V3(0.36f, 0.22f, 0.07f), 0.25f, V3(0.15f, 0.08f, 0.03f), 8, 4, 0, 0, 0.12f, 0.6f, 0.15f, 0.06f);
  MT.litter2 = A(1, V3(0.42f, 0.30f, 0.06f), 0.7f, V3(0.30f, 0.14f, 0.04f), 0.35f, V3(0.22f, 0.12f, 0.04f), 7, 4, 0, 0, 0.08f, 0.5f, 0.2f, 0.05f);
  MT.twig = A(4, V3(0.12f, 0.085f, 0.055f), 0.9f, V3(0.05f, 0.035f, 0.025f), 0, z, 60, 0.25f, 0, 0.2f);
  MT.bark = A(4, V3(0.16f, 0.12f, 0.085f), 0.9f, V3(0.05f, 0.04f, 0.03f), 0, z, 9, 0.2f, 0, 0.9f);
  MT.barkBirch = A(4, V3(0.72f, 0.70f, 0.64f), 0.7f, V3(0.05f, 0.045f, 0.04f), 0, z, 9, 0.2f, 1, 0.3f);
  MT.barkDark = A(4, V3(0.07f, 0.055f, 0.04f), 0.9f, V3(0.03f, 0.025f, 0.02f), 0, z, 24, 0.3f, 0, 0.6f);
  MT.mulch = A(4, V3(0.17f, 0.10f, 0.055f), 0.9f, V3(0.07f, 0.04f, 0.02f), 0, z, 80, 0.3f, 0, 0.0f);
  MT.hosta = A(1, V3(0.030f, 0.105f, 0.045f), 0.62f, V3(0.05f, 0.14f, 0.05f), 0.4f, V3(0.045f, 0.13f, 0.055f), 9, 0, 1, 0, 0.10f, 0.15f, 0.22f);
  MT.hostaVar = A(1, V3(0.04f, 0.13f, 0.03f), 0.55f, V3(0.08f, 0.18f, 0.04f), 0.45f, V3(0.07f, 0.17f, 0.05f), 8, 0, 1, 0.8f, 0.07f, 0.12f, 0.25f);
  MT.fern = A(1, V3(0.04f, 0.14f, 0.02f), 0.5f, V3(0.09f, 0.21f, 0.03f), 0.5f, V3(0.12f, 0.2f, 0.06f), 14, 3, 0, 0, 0, 0.12f, 0.3f);
  MT.sprig = A(1, V3(0.025f, 0.085f, 0.018f), 0.32f, V3(0.07f, 0.17f, 0.03f), 0.35f, V3(0.12f, 0.2f, 0.07f), 6, 3, 0, 0, 0, 0.08f, 0.2f);
  MT.sprigDark = A(1, V3(0.02f, 0.06f, 0.016f), 0.3f, V3(0.04f, 0.11f, 0.025f), 0.3f, V3(0.08f, 0.15f, 0.05f), 6, 3, 0, 0, 0, 0.05f, 0.18f);
  MT.treeLeaf = A(1, V3(0.04f, 0.12f, 0.02f), 0.42f, V3(0.10f, 0.20f, 0.03f), 0.5f, V3(0.18f, 0.27f, 0.10f), 7, 3.5f, 0, 0, 0.03f, 0.12f, 0.28f);
  MT.birchLeaf = A(1, V3(0.06f, 0.16f, 0.02f), 0.42f, V3(0.16f, 0.24f, 0.03f), 0.55f, V3(0.2f, 0.3f, 0.1f), 7, 3.5f, 0, 0, 0, 0.1f, 0.3f, 0.05f);
  // petals type 3: A(3, base, rough, main, trans, tip, spots, veinStrength, tipAmount)
  MT.petPoppy = A(3, V3(0.03f, 0.01f, 0.02f), 0.5f, V3(0.86f, 0.10f, 0.025f), 0.62f, V3(0.9f, 0.2f, 0.05f), 0, 0.5f, 0.3f, 0, 0, 0, 0.5f);
  MT.poppyPod = A(0, V3(0.16f, 0.24f, 0.12f), 0.6f, V3(0.25f, 0.3f, 0.2f), 0.1f, z, 0.5f, 60, 0.5f);
  MT.petFox = A(3, V3(0.75f, 0.62f, 0.70f), 0.55f, V3(0.62f, 0.13f, 0.42f), 0.5f, V3(0.75f, 0.3f, 0.55f), 1.0f, 0.3f, 0.5f, 0, 0, 0, 0.45f);
  MT.foxLeaf = A(1, V3(0.05f, 0.13f, 0.03f), 0.75f, V3(0.08f, 0.17f, 0.04f), 0.4f, V3(0.14f, 0.22f, 0.1f), 9, 3, 0, 0, 0.06f, 0.2f, 0.25f, 0.03f);
  MT.petAllium = A(3, V3(0.35f, 0.12f, 0.5f), 0.55f, V3(0.42f, 0.14f, 0.62f), 0.5f, V3(0.6f, 0.4f, 0.75f), 0, 0.3f, 0.3f, 0, 0, 0, 0.5f);
  MT.lavLeaf = A(1, V3(0.14f, 0.19f, 0.13f), 0.8f, V3(0.2f, 0.25f, 0.17f), 0.3f, V3(0.25f, 0.3f, 0.2f), 1, 0, 1, 0, 0, 0.1f, 0.2f);
  MT.petLav = A(3, V3(0.22f, 0.16f, 0.42f), 0.7f, V3(0.30f, 0.20f, 0.60f), 0.35f, V3(0.4f, 0.3f, 0.7f), 0, 0.3f, 0.4f, 0, 0, 0, 0.4f);
  MT.petPink = A(3, V3(0.55f, 0.12f, 0.25f), 0.55f, V3(0.80f, 0.25f, 0.45f), 0.5f, V3(0.9f, 0.5f, 0.65f), 0, 0.6f, 0.4f, 0, 0, 0, 0.45f);
  MT.petPurple = A(3, V3(0.75f, 0.6f, 0.1f), 0.55f, V3(0.25f, 0.10f, 0.50f), 0.45f, V3(0.35f, 0.18f, 0.6f), 0, 0.7f, 0.4f, 0, 0, 0, 0.45f);
  MT.petOrange = A(3, V3(0.55f, 0.12f, 0.01f), 0.55f, V3(0.90f, 0.42f, 0.02f), 0.5f, V3(0.95f, 0.6f, 0.05f), 0, 0.6f, 0.4f, 0, 0, 0, 0.45f);
  MT.petCosmos = A(3, V3(0.75f, 0.35f, 0.55f), 0.55f, V3(0.86f, 0.45f, 0.68f), 0.6f, V3(0.9f, 0.7f, 0.82f), 0, 0.8f, 0.4f, 0, 0, 0, 0.5f);
  MT.lilyPad = A(1, V3(0.03f, 0.12f, 0.03f), 0.22f, V3(0.08f, 0.18f, 0.03f), 0.3f, V3(0.12f, 0.22f, 0.08f), 16, 0, 2, 0, 0.06f, 0.10f, 0.15f);
  MT.petLily = A(3, V3(0.85f, 0.75f, 0.45f), 0.5f, V3(0.88f, 0.84f, 0.86f), 0.55f, V3(0.9f, 0.5f, 0.68f), 0, 0.3f, 0.7f, 0, 0, 0, 0.5f);
  MT.reed = A(2, V3(0.05f, 0.14f, 0.03f), 0.4f, V3(0.13f, 0.21f, 0.04f), 0.5f, V3(0.25f, 0.28f, 0.12f), 0, 0, 0, 0, 0, 0, 0.3f);
  MT.cattail = A(0, V3(0.14f, 0.075f, 0.035f), 0.95f, V3(0.08f, 0.04f, 0.02f), 0, z, 0.8f, 400, 0.15f);
  MT.irisLeaf = A(2, V3(0.05f, 0.14f, 0.06f), 0.4f, V3(0.08f, 0.18f, 0.07f), 0.45f, V3(0.2f, 0.26f, 0.14f), 0, 0, 0, 0, 0, 0, 0.3f);
  MT.petIris = A(3, V3(0.85f, 0.75f, 0.1f), 0.5f, V3(0.22f, 0.14f, 0.62f), 0.5f, V3(0.3f, 0.2f, 0.7f), 0, 0.8f, 0.3f, 0, 0, 0, 0.45f);
  // wood: A(7, colA, rough, colB, 0, -, grainScale, -, -, algae)
  MT.wood = A(7, V3(0.30f, 0.27f, 0.23f), 0.85f, V3(0.13f, 0.11f, 0.09f), 0, z, 1.0f, 0, 0, 1.0f);
  MT.woodBench = A(7, V3(0.33f, 0.20f, 0.10f), 0.6f, V3(0.16f, 0.09f, 0.045f), 0, z, 1.0f, 0, 0, 0.3f);
  MT.woodPaint = A(19, V3(0.74f, 0.75f, 0.72f), 0.45f, V3(0.25f, 0.20f, 0.15f), 0, z, 0.8f, 0, 0, 0.6f);
  MT.brick = A(8, V3(0.27f, 0.115f, 0.08f), 0.9f, V3(0.37f, 0.20f, 0.13f), 0, V3(0.13f, 0.085f, 0.07f), 0.225f, 0.075f, 0, 0.45f);
  MT.brickUnit = A(0, V3(0.33f, 0.14f, 0.09f), 0.9f, V3(0.20f, 0.10f, 0.07f), 0, V3(0.07f, 0.085f, 0.04f), 0.8f, 30, 0.3f, 0.35f);
  MT.iron = A(19, V3(0.03f, 0.07f, 0.045f), 0.4f, V3(0.25f, 0.10f, 0.04f), 0, z, 0.5f, 0, 0, 0.2f);
  MT.whiteMetal = A(19, V3(0.72f, 0.74f, 0.71f), 0.4f, V3(0.30f, 0.16f, 0.08f), 0, z, 0.55f, 0, 0, 0.7f);
  MT.galv = A(19, V3(0.42f, 0.44f, 0.45f), 0.35f, V3(0.30f, 0.30f, 0.30f), 0, z, 0.5f, 0, 0, 0.1f);
  MT.glass = A(9, V3(0.8f, 0.85f, 0.82f), 0.05f, V3(0.64f, 0.68f, 0.65f), 0, z, 0.85f, 0, 0, 0, 0, 0, 0.5f);
  MT.water = A(10, V3(0.02f, 0.04f, 0.03f), 0.02f, V3(0.47f, 0.47f, 0.47f), 0, z, 0, 0, 0, 0, 0, 0, 0.5f);
  MT.paving = A(21, V3(0.30f, 0.28f, 0.25f), 0.8f, V3(0.40f, 0.36f, 0.30f), 0, z, 0.60f, 0.40f, 1.0f, 1.0f);
  MT.terracotta = A(0, V3(0.48f, 0.21f, 0.11f), 0.85f, V3(0.60f, 0.42f, 0.32f), 0, V3(0.14f, 0.2f, 0.07f), 0.7f, 18, 0.3f, 0.9f);
  MT.banana = A(1, V3(0.045f, 0.16f, 0.02f), 0.28f, V3(0.10f, 0.24f, 0.03f), 0.5f, V3(0.2f, 0.3f, 0.08f), 46, 6, 4, 0, 0, 0.10f, 0.3f);
  MT.monstera = A(1, V3(0.02f, 0.085f, 0.02f), 0.18f, V3(0.04f, 0.13f, 0.025f), 0.3f, V3(0.12f, 0.2f, 0.07f), 8, 3.2f, 3, 0, 0, 0.03f, 0.2f);
  MT.alocasia = A(1, V3(0.02f, 0.075f, 0.03f), 0.2f, V3(0.035f, 0.11f, 0.035f), 0.3f, V3(0.45f, 0.55f, 0.42f), 6, 2.2f, 0, 0, 0, 0.03f, 0.2f);
  MT.calathea = A(1, V3(0.02f, 0.08f, 0.025f), 0.3f, V3(0.05f, 0.16f, 0.04f), 0.35f, V3(0.55f, 0.50f, 0.50f), 13, 2.5f, 0, 0, 0, 0.05f, 0.22f);
  MT.bromeliad = A(2, V3(0.06f, 0.16f, 0.04f), 0.3f, V3(0.10f, 0.20f, 0.05f), 0.35f, V3(0.62f, 0.06f, 0.08f), 0, 0, 0, 0, 0, 0, 0.25f);
  MT.orchidLeaf = A(16, V3(0.03f, 0.11f, 0.025f), 0.25f, V3(0.05f, 0.15f, 0.03f), 0.1f, V3(0.05f, 0.15f, 0.03f), 0, 0, 0, 0);
  MT.petOrchid = A(3, V3(0.85f, 0.80f, 0.55f), 0.45f, V3(0.88f, 0.86f, 0.88f), 0.5f, V3(0.9f, 0.7f, 0.85f), 0, 0.25f, 0.25f, 0, 0, 0, 0.5f);
  MT.petOrchidLip = A(3, V3(0.85f, 0.65f, 0.05f), 0.45f, V3(0.60f, 0.04f, 0.32f), 0.4f, V3(0.75f, 0.1f, 0.45f), 0.6f, 0.4f, 0.3f, 0, 0, 0, 0.5f);
  MT.sarra = A(1, V3(0.10f, 0.22f, 0.03f), 0.35f, V3(0.32f, 0.36f, 0.05f), 0.5f, V3(0.42f, 0.03f, 0.04f), 16, 1.2f, 0, 0, 0, 0.1f, 0.3f);
  MT.sarraHood = A(1, V3(0.30f, 0.34f, 0.06f), 0.35f, V3(0.45f, 0.36f, 0.10f), 0.55f, V3(0.50f, 0.03f, 0.05f), 14, 2.0f, 0, 0, 0, 0.1f, 0.3f);
  MT.flytrap = A(1, V3(0.10f, 0.22f, 0.03f), 0.35f, V3(0.2f, 0.3f, 0.05f), 0.45f, V3(0.55f, 0.10f, 0.08f), 0, 0, 1, 0, 0, 0, 0.3f);
  MT.moss = A(0, V3(0.03f, 0.075f, 0.012f), 0.95f, V3(0.075f, 0.12f, 0.02f), 0.1f, z, 0.9f, 120, 0.1f);
  // cactus type 16: A(16, colA, rough, colB, trans, tipCol, ribs, areoleFreq, tipAmount, glaucous)
  MT.cactus = A(16, V3(0.04f, 0.13f, 0.03f), 0.4f, V3(0.07f, 0.17f, 0.04f), 0, z, 21, 11, 0, 0);
  MT.cactusCol = A(16, V3(0.06f, 0.15f, 0.07f), 0.45f, V3(0.09f, 0.19f, 0.10f), 0, z, 9, 26, 0, 0);
  MT.spine = A(0, V3(0.62f, 0.50f, 0.25f), 0.5f, V3(0.75f, 0.68f, 0.5f), 0.2f, z, 0.5f, 200, 0.4f);
  MT.opuntia = A(16, V3(0.07f, 0.17f, 0.06f), 0.45f, V3(0.10f, 0.21f, 0.08f), 0, V3(0.1f, 0.2f, 0.08f), 0, 0, 0, 0.25f);
  MT.agave = A(16, V3(0.07f, 0.16f, 0.12f), 0.4f, V3(0.10f, 0.20f, 0.15f), 0.05f, V3(0.25f, 0.08f, 0.04f), 0, 0, 0.3f, 0.45f);
  MT.echeveria = A(16, V3(0.12f, 0.24f, 0.18f), 0.5f, V3(0.16f, 0.28f, 0.22f), 0.12f, V3(0.75f, 0.25f, 0.30f), 0, 0, 0.75f, 0.6f);
  MT.aloe = A(16, V3(0.05f, 0.16f, 0.05f), 0.3f, V3(0.09f, 0.21f, 0.06f), 0.1f, V3(0.4f, 0.2f, 0.08f), 0, 0, 0.3f, 0.1f);
  MT.lithops = A(5, V3(0.36f, 0.33f, 0.30f), 0.6f, z, 0, z, 5.0f, 0, 0, 0);
  MT.cactusFl = A(3, V3(0.9f, 0.7f, 0.1f), 0.5f, V3(0.92f, 0.20f, 0.42f), 0.5f, V3(0.95f, 0.4f, 0.6f), 0, 0.4f, 0.4f, 0, 0, 0, 0.5f);
  // insects etc.
  MT.ladyShell = A(11, V3(0.72f, 0.04f, 0.02f), 0.12f, V3(0.01f, 0.01f, 0.01f), 0, z, 0, 0, 0.06f);
  MT.ladyBlack = A(0, V3(0.012f, 0.012f, 0.014f), 0.2f, V3(0.02f, 0.02f, 0.02f), 0, z, 0.1f, 100, 1.0f);
  MT.beeBody = A(11, V3(0.02f, 0.016f, 0.012f), 0.9f, V3(0.75f, 0.50f, 0.03f), 0, V3(0.8f, 0.78f, 0.7f), 1, 2.6f, 0);
  MT.wingClear = A(12, V3(0.5f, 0.5f, 0.5f), 0.15f, V3(0.47f, 0.47f, 0.47f), 0.8f, z, 1, 0, 0, 0, 0, 0.30f, 0.45f);
  MT.bfWing = A(12, V3(0.90f, 0.38f, 0.02f), 0.6f, V3(0.80f, 0.20f, 0.01f), 0.3f, V3(0.02f, 0.015f, 0.01f), 0, 0, 0, 0, 0, 1, 0.12f);
  MT.bfWing2 = A(12, V3(0.86f, 0.86f, 0.78f), 0.6f, V3(0.80f, 0.80f, 0.60f), 0.4f, V3(0.12f, 0.12f, 0.10f), 0, 0, 0, 0, 0, 1, 0.2f);
  MT.bfBody = A(0, V3(0.02f, 0.016f, 0.012f), 0.8f, V3(0.08f, 0.06f, 0.04f), 0, z, 0.5f, 300, 0.2f);
  MT.dfBody = A(11, V3(0.02f, 0.25f, 0.55f), 0.2f, V3(0.01f, 0.02f, 0.03f), 0, z, 2, 7, 0.25f);
  MT.dfEye = A(0, V3(0.05f, 0.22f, 0.20f), 0.1f, V3(0.02f, 0.1f, 0.15f), 0, z, 0.5f, 200, 1.0f);
  MT.koi = A(13, V3(0.8f, 0.3f, 0.05f), 0.25f, z, 0, z, 0);
  MT.koiFin = A(13, V3(0.8f, 0.3f, 0.05f), 0.3f, z, 0, z, 1, 0, 0, 0, 0, 0, 0.3f);
  MT.fur = A(14, V3(0.20f, 0.15f, 0.10f), 0.6f, V3(0.045f, 0.035f, 0.03f), 0.12f, V3(0.75f, 0.72f, 0.66f), 60, 14, 0);
  MT.furPaw = A(14, V3(0.24f, 0.18f, 0.12f), 0.6f, V3(0.05f, 0.04f, 0.03f), 0.2f, V3(0.80f, 0.77f, 0.70f), 60, 30, 0.55f);
  MT.denim = A(15, V3(0.035f, 0.06f, 0.14f), 0.95f, V3(0.10f, 0.14f, 0.24f), 0, z, 1.0f);
  MT.shoe = A(0, V3(0.62f, 0.60f, 0.56f), 0.6f, V3(0.35f, 0.33f, 0.30f), 0, V3(0.25f, 0.22f, 0.18f), 0.6f, 40, 0.4f, 0.9f);
  MT.shoeSole = A(0, V3(0.75f, 0.74f, 0.70f), 0.5f, V3(0.5f, 0.48f, 0.42f), 0, V3(0.2f, 0.17f, 0.13f), 0.5f, 60, 0.5f, 1.0f);
  MT.antBody = A(0, V3(0.02f, 0.012f, 0.008f), 0.25f, V3(0.05f, 0.02f, 0.01f), 0, z, 0.3f, 500, 1.0f);
  MT.whisker = A(0, V3(0.85f, 0.85f, 0.82f), 0.3f, V3(0.8f, 0.8f, 0.8f), 0.3f, z, 0, 10, 0.6f, 0, 0, 1, 0.8f);
  MT.label = A(17, V3(0.02f, 0.022f, 0.02f), 0.62f, V3(0.82f, 0.82f, 0.78f), 0, z, 16);
  MT.labelStake = A(19, V3(0.05f, 0.05f, 0.05f), 0.4f, V3(0.2f, 0.12f, 0.06f), 0, z, 0.3f);
  MT.dew = A(18, V3(1, 1, 1), 0.0f, V3(0.47f, 0.47f, 0.47f), 0, z, 0, 0, 0, 0, 0, 0, 0.5f);
  MT.hose = A(0, V3(0.05f, 0.22f, 0.07f), 0.35f, V3(0.04f, 0.16f, 0.05f), 0, V3(0.2f, 0.18f, 0.12f), 0.4f, 80, 0.8f, 0.5f);
  MT.mushCap = A(0, V3(0.42f, 0.17f, 0.06f), 0.4f, V3(0.60f, 0.38f, 0.18f), 0.05f, z, 0.8f, 40, 0.8f);
  MT.mushStem = A(0, V3(0.72f, 0.68f, 0.58f), 0.8f, V3(0.55f, 0.50f, 0.40f), 0.2f, z, 0.6f, 60, 0.3f);
  MT.snailShell = A(4, V3(0.42f, 0.28f, 0.12f), 0.35f, V3(0.14f, 0.08f, 0.04f), 0, z, 160, 0.15f, 0, 0);
  MT.snailBody = A(0, V3(0.22f, 0.19f, 0.14f), 0.25f, V3(0.12f, 0.10f, 0.08f), 0.2f, z, 0.6f, 300, 1.0f);
  MT.robin = A(0, V3(0.20f, 0.15f, 0.10f), 0.9f, V3(0.12f, 0.09f, 0.06f), 0, z, 0.6f, 60, 0.2f);
}

static inline M34 place(V3 p, float yaw, float s = 1, float tiltX = 0, float tiltZ = 0) {
  return mTrans(p) * mRotY(yaw) * mRotX(tiltX) * mRotZ(tiltZ) * mScale(s);
}
static const float TAU = 6.2831853f;

// ---------------------------------------------------------------- grasses
static inline Mesh grassClump(Rng& R, float h, int nb, float spread, float flop, int mat) {
  Mesh m;
  for (int i = 0; i < nb; i++) {
    LeafP L;
    L.len = h * R.r(0.5f, 1.3f); L.wid = R.r(0.0022f, 0.0040f) * (0.8f + 3.0f * h); L.nu = 2; L.nv = h > 0.12f ? 8 : 5;
    L.prof = profStrap(R.r(1.5f, 4.0f), 0.85f);
    L.pitch = R.r(0.95f, 1.52f) - flop * R.f(); L.droop = R.r(0.05f, 0.9f) * (0.5f + flop) * (0.6f + 4.0f * h); L.droopPow = R.r(1.2f, 2.6f);
    L.fold = R.r(0.15f, 0.5f); L.twist = R.r(-0.9f, 0.9f); L.side = R.r(-0.4f, 0.4f); L.w = R.f();
    float a = R.f() * TAU, rr = spread * sqrtf(R.f());
    addLeaf(m, L, mTrans(V3(cosf(a) * rr, -0.003f, sinf(a) * rr)) * mRotY(R.f() * TAU), mat);
  }
  return m;
}
// a square sod tile: blades uniformly spread over size x size (BLAS handles the density; few overlapping instances)
static inline Mesh grassTile(Rng& R, float size, float h, int nb, float flop, int mat) {
  Mesh m;
  for (int i = 0; i < nb; i++) {
    LeafP L;
    float hh = h * R.r(0.45f, 1.25f) * (R.chance(0.04f) ? 1.5f : 1.0f);
    L.len = hh; L.wid = R.r(0.0020f, 0.0040f); L.nu = 2; L.nv = 4;
    L.prof = profStrap(R.r(1.5f, 4.0f), 0.85f);
    L.pitch = R.r(0.9f, 1.52f) - flop * R.f(); L.droop = R.r(0.05f, 0.9f) * (0.5f + flop) * (0.6f + 4.0f * hh); L.droopPow = R.r(1.2f, 2.6f);
    L.fold = R.r(0.15f, 0.5f); L.twist = R.r(-0.9f, 0.9f); L.side = R.r(-0.4f, 0.4f); L.w = R.f();
    addLeaf(m, L, mTrans(V3(R.r(-0.5f, 0.5f) * size, -0.003f, R.r(-0.5f, 0.5f) * size)) * mRotY(R.f() * TAU), mat);
  }
  return m;
}
static inline Mesh tallGrass(Rng& R, float h, int nb, int nheads) {
  Mesh m = grassClump(R, h, nb, 0.02f + 0.05f * h, 0.35f, MT.grassTall);
  for (int i = 0; i < nheads; i++) {
    float hh = h * R.r(1.1f, 1.6f), lean = R.r(0.05f, 0.35f), az = R.f() * TAU;
    V3 dir(cosf(az), 0, sinf(az));
    V3 a = dir * R.r(0, 0.02f), d = a + V3(0, hh, 0) + dir * (hh * lean);
    auto pts = curvePts(a, a + V3(0, hh * 0.4f, 0), a + V3(0, hh * 0.8f, 0) + dir * (hh * lean * 0.4f), d, 8);
    addTube(m, pts, [](float t) { return 0.0009f * (1.2f - 0.7f * t); }, 3, MT.stemPale, M34(), R.f(), false);
    int ns = R.i(8) + 10;
    for (int s = 0; s < ns; s++) { // spikelets
      float t = 0.72f + 0.28f * s / ns; int i0 = std::min((int)(t * 8), 7);
      V3 p = lerp(pts[i0], pts[i0 + 1], t * 8 - i0);
      LeafP L; L.len = R.r(0.006f, 0.014f); L.wid = R.r(0.002f, 0.0035f); L.nu = 2; L.nv = 2; L.prof = profEll(1, 0.8f); L.pitch = R.r(0.2f, 1.2f); L.droop = R.r(0, 0.8f); L.w = R.f();
      addLeaf(m, L, mTrans(p) * mRotY(R.f() * TAU), MT.seed);
    }
  }
  return m;
}
// ---------------------------------------------------------------- small lawn plants
static inline Mesh cloverPatch(Rng& R, int n, bool flower) {
  Mesh m;
  for (int i = 0; i < n; i++) {
    float a = R.f() * TAU, rr = 0.035f * sqrtf(R.f()), h = R.r(0.025f, 0.075f);
    V3 b(cosf(a) * rr, 0, sinf(a) * rr), top = b + V3(R.r(-0.012f, 0.012f), h, R.r(-0.012f, 0.012f));
    addTube(m, {b, lerp(b, top, 0.5f) + V3(0.003f, 0, 0), top}, [](float) { return 0.0006f; }, 3, MT.stemGreen, M34(), R.f(), false);
    float yaw = R.f() * TAU, sz = R.r(0.009f, 0.016f); float w = R.f();
    for (int k = 0; k < 3; k++) {
      LeafP L; L.len = sz; L.wid = sz * 0.95f; L.nu = 4; L.nv = 4; L.prof = profEll(1.5f, 0.55f); L.pitch = R.r(0.0f, 0.35f); L.droop = R.r(-0.2f, 0.4f); L.fold = R.r(0.1f, 0.45f); L.w = w;
      addLeaf(m, L, mTrans(top) * mRotY(yaw + k * TAU / 3), MT.clover);
    }
  }
  if (flower) {
    V3 top(R.r(-0.02f, 0.02f), R.r(0.07f, 0.11f), R.r(-0.02f, 0.02f));
    addTube(m, {V3(0, 0, 0), top * 0.5f + V3(0.004f, 0, 0), top}, [](float) { return 0.0008f; }, 3, MT.stemGreen, M34(), R.f(), false);
    for (int k = 0; k < 46; k++) {
      float u = R.f() * 2 - 1, a = R.f() * TAU, s = sqrtf(1 - u * u);
      V3 d(s * cosf(a), u * 0.9f + 0.25f, s * sinf(a));
      LeafP L; L.len = 0.006f; L.wid = 0.0028f; L.nu = 2; L.nv = 2; L.pitch = 0; L.droop = -0.4f; L.w = R.f();
      addLeaf(m, L, mLookZ(norm(d), top + d * 0.004f), MT.cloverFl);
    }
  }
  return m;
}
static inline void addStem(Mesh& m, V3 a, V3 b, float bend, float r0, float r1, int mat, Rng& R, std::vector<V3>* outPts = nullptr, int sides = 4) {
  V3 side = norm(cross(b - a, V3(R.r(-1, 1), 0.2f, R.r(-1, 1))));
  float L = len(b - a);
  auto pts = curvePts(a, lerp(a, b, 0.33f) + side * (bend * L), lerp(a, b, 0.66f) + side * (bend * L * 0.6f), b, 8);
  addTube(m, pts, [=](float t) { return lerpf(r0, r1, t); }, sides, mat, M34(), R.f(), false);
  if (outPts) *outPts = pts;
}
// composite (daisy-like) flower head at origin facing +Y
static inline void addDaisyHead(Mesh& m, Rng& R, const M34& xf, int nPet, float petLen, float petWid, float discR, float discH, int petMat, int discMat, float droop, int rings = 1) {
  addBlob(m, V3(0, discH * 0.3f, 0), V3(discR, discH, discR), 10, 5, 0.12f, 9.0f, R.f() * 10, discMat, xf, R.f());
  for (int rg = 0; rg < rings; rg++)
    for (int i = 0; i < nPet; i++) {
      LeafP L; L.len = petLen * R.r(0.85f, 1.1f) * (1 - 0.18f * rg); L.wid = petWid * R.r(0.85f, 1.15f); L.nu = 2; L.nv = 4; L.prof = profEll(1.25f, 0.5f, 0.3f);
      L.pitch = 0.12f + 0.3f * rg + R.r(-0.12f, 0.12f) - droop * 0.4f; L.droop = droop * R.r(0.6f, 1.3f); L.fold = R.r(-0.25f, 0.1f); L.twist = R.r(-0.25f, 0.25f); L.w = R.f();
      addLeaf(m, L, xf * mRotY((i + 0.5f * rg + R.r(-0.15f, 0.15f)) * TAU / nPet) * mTrans(V3(0, 0, discR * 0.8f)), petMat);
    }
}
static inline Mesh lawnDaisy(Rng& R) {
  Mesh m;
  for (int i = 0; i < 6; i++) { // spoon leaves, flat rosette
    LeafP L; L.len = R.r(0.02f, 0.04f); L.wid = L.len * 0.4f; L.nu = 2; L.nv = 4; L.prof = profEll(1.8f, 0.6f); L.pitch = R.r(0.05f, 0.3f); L.droop = R.r(0.1f, 0.4f); L.w = R.f();
    addLeaf(m, L, mRotY(R.f() * TAU), MT.dandLeaf);
  }
  int nf = 1 + R.i(3);
  for (int f = 0; f < nf; f++) {
    V3 top(R.r(-0.02f, 0.02f), R.r(0.045f, 0.085f), R.r(-0.02f, 0.02f));
    addStem(m, V3(0, 0, 0), top, 0.12f, 0.0008f, 0.0007f, MT.stemGreen, R, nullptr, 3);
    addDaisyHead(m, R, mAlignY(norm(top + V3(R.r(-0.02f, 0.02f), 0.05f, R.r(-0.02f, 0.02f))), top), 22 + R.i(8), 0.0085f, 0.0026f, 0.0038f, 0.0022f, MT.petWhite, MT.discYellow, R.r(-0.1f, 0.25f));
  }
  return m;
}
static inline Mesh dandelion(Rng& R, int kind) { // 0 flower, 1 clock, 2 both
  Mesh m;
  int nl = 8 + R.i(6);
  for (int i = 0; i < nl; i++) {
    LeafP L; L.len = R.r(0.07f, 0.15f); L.wid = L.len * R.r(0.2f, 0.3f); L.nu = 4; L.nv = 14;
    float teeth = R.r(5, 8), ph = R.f();
    L.prof = [=](float v) { float s = powf(fmaxf(sinf(PI * powf(v, 1.5f)), 0.0f), 0.7f); float t = fabsf(fmodf(v * teeth + ph, 1.0f) - 0.35f) / 0.65f; return s * (0.28f + 0.72f * fminf(1.0f, t * 1.3f)) + 0.06f * (1 - v); };
    L.pitch = R.r(0.05f, 0.5f); L.droop = R.r(0.2f, 0.8f); L.fold = R.r(0.1f, 0.4f); L.wave = 0.05f; L.waveN = teeth; L.twist = R.r(-0.3f, 0.3f); L.w = R.f();
    addLeaf(m, L, mRotY(i * 2.4f + R.r(-0.2f, 0.2f)), MT.dandLeaf);
  }
  int ns = kind == 2 ? 3 : 1 + R.i(2);
  for (int s = 0; s < ns; s++) {
    bool clock = kind == 1 || (kind == 2 && s > 0);
    V3 top(R.r(-0.05f, 0.05f), R.r(0.11f, 0.24f), R.r(-0.05f, 0.05f));
    addStem(m, V3(R.r(-0.005f, 0.005f), 0, R.r(-0.005f, 0.005f)), top, 0.08f, 0.0019f, 0.0015f, MT.stemPale, R, nullptr, 5);
    V3 up = norm(top + V3(R.r(-0.03f, 0.03f), 0.12f, R.r(-0.03f, 0.03f)));
    M34 hx = mAlignY(up, top);
    if (!clock) {
      addRevolve(m, {{0.0015f, -0.004f}, {0.004f, 0.0f}, {0.0045f, 0.004f}, {0.003f, 0.006f}}, 8, 3, MT.stemGreen, hx);
      for (int rg = 0; rg < 4; rg++)
        for (int i = 0; i < 26 - rg * 5; i++) {
          LeafP L; L.len = 0.016f * (1 - 0.2f * rg) * R.r(0.8f, 1.1f); L.wid = 0.0026f; L.nu = 2; L.nv = 3; L.prof = profStrap(6, 0.9f);
          L.pitch = 0.15f + 0.38f * rg + R.r(-0.1f, 0.1f); L.droop = R.r(-0.2f, 0.4f); L.w = R.f();
          addLeaf(m, L, hx * mTrans(V3(0, 0.005f, 0)) * mRotY(R.f() * TAU) * mTrans(V3(0, 0, 0.001f * (4 - rg))), MT.petYellow);
        }
    } else {
      addBlob(m, V3(0, 0, 0), V3(0.003f, 0.0025f, 0.003f), 8, 4, 0.1f, 5, 1, MT.stemPale, hx);
      int nseed = 110;
      for (int i = 0; i < nseed; i++) {
        float u = 1 - 1.75f * (i + 0.5f) / nseed, a = i * 2.39996f, sn = sqrtf(fmaxf(1 - u * u, 0.0f));
        V3 d = norm(V3(sn * cosf(a), u, sn * sinf(a)) + V3(R.r(-0.06f, 0.06f), R.r(-0.06f, 0.06f), R.r(-0.06f, 0.06f)));
        M34 sx = hx * mLookZ(d, d * 0.003f);
        float rl = R.r(0.011f, 0.014f);
        LeafP L; L.len = rl; L.wid = 0.00022f; L.nu = 1; L.nv = 1; L.prof = [](float) { return 1.0f; }; L.pitch = 0; L.droop = 0; L.fold = 0; L.w = R.f();
        addLeaf(m, L, sx, MT.pappus);
        addLeaf(m, L, sx * mRotZ(PI / 2), MT.pappus);
        for (int k = 0; k < 7; k++) { // pappus umbrella
          LeafP H; H.len = 0.0055f; H.wid = 0.00016f; H.nu = 1; H.nv = 1; H.prof = [](float) { return 1.0f; }; H.pitch = 0.5f; H.droop = 0; H.fold = 0; H.w = R.f();
          addLeaf(m, H, sx * mTrans(V3(0, 0, rl)) * mRotZ(k * TAU / 7) * mRotX(-PI / 2 + 0.55f) * mRotZ(PI / 2), MT.pappus);
        }
      }
    }
  }
  return m;
}
// ---------------------------------------------------------------- stones / litter
static inline Mesh pebble(Rng& R, int nu = 12, int nv = 8, int mat = -1) {
  Mesh m;
  addBlob(m, V3(0, 0, 0), V3(1.0f, R.r(0.42f, 0.8f), R.r(0.65f, 1.0f)), nu, nv, R.r(0.12f, 0.28f), R.r(1.1f, 2.0f), R.f() * 100, mat < 0 ? MT.stone : mat);
  return m;
}
static inline Mesh rock(Rng& R, int mat, float flat = 0.6f) {
  Mesh m;
  addBlob(m, V3(0, 0, 0), V3(1.0f, R.r(0.4f, 0.75f) * flat / 0.6f, R.r(0.65f, 1.0f)), 20, 14, R.r(0.2f, 0.36f), R.r(1.3f, 2.2f), R.f() * 100, mat);
  return m;
}
static inline Mesh deadLeaf(Rng& R, int mat) {
  Mesh m;
  LeafP L; L.len = R.r(0.05f, 0.09f); L.wid = L.len * R.r(0.5f, 0.75f); L.nu = 6; L.nv = 9; L.prof = profEll(R.r(0.7f, 1.0f), R.r(0.5f, 0.8f));
  L.pitch = R.r(-0.1f, 0.3f); L.droop = R.r(-0.9f, 1.2f); L.fold = R.r(-0.3f, 0.6f); L.curl = R.r(-0.6f, 0.6f); L.wave = R.r(0.05f, 0.2f); L.waveN = R.r(2, 5); L.twist = R.r(-1.2f, 1.2f); L.side = R.r(-0.5f, 0.5f); L.w = R.f();
  addLeaf(m, L, mTrans(V3(0, 0.004f, -L.len * 0.5f)), mat);
  addTube(m, {V3(0, 0.004f, -L.len * 0.5f), V3(R.r(-0.004f, 0.004f), 0.006f, -L.len * 0.5f - 0.015f)}, [](float) { return 0.0006f; }, 3, MT.twig, M34(), 0, false);
  return m;
}
static inline Mesh twigMesh(Rng& R) {
  Mesh m;
  V3 a(-0.5f, 0.03f, 0), b(0.5f, 0.03f, R.r(-0.1f, 0.1f));
  addStem(m, a, b, R.r(-0.12f, 0.12f), 0.035f, 0.02f, MT.twig, R, nullptr, 5);
  if (R.chance(0.6f)) addStem(m, lerp(a, b, R.r(0.3f, 0.7f)), lerp(a, b, R.r(0.5f, 0.9f)) + V3(0, R.r(0, 0.1f), R.r(-0.3f, 0.3f)), 0.05f, 0.02f, 0.012f, MT.twig, R, nullptr, 4);
  return m;
}
static inline Mesh barkChip(Rng& R) {
  Mesh m;
  addBlob(m, V3(0, 0.12f, 0), V3(1.0f, R.r(0.12f, 0.25f), R.r(0.4f, 0.8f)), 8, 5, 0.35f, 2.5f, R.f() * 50, MT.mulch);
  return m;
}
static inline Mesh mushroom(Rng& R) {
  Mesh m;
  int n = 3 + R.i(4);
  for (int i = 0; i < n; i++) {
    float h = R.r(0.03f, 0.07f), cr = R.r(0.012f, 0.028f);
    V3 b(R.r(-0.05f, 0.05f), 0, R.r(-0.05f, 0.05f)), t = b + V3(R.r(-0.012f, 0.012f), h, R.r(-0.012f, 0.012f));
    addStem(m, b, t, 0.06f, 0.0045f, 0.0035f, MT.mushStem, R, nullptr, 6);
    M34 x = mAlignY(norm(t - b + V3(R.r(-0.01f, 0.01f), 0.02f, R.r(-0.01f, 0.01f))), t);
    addRevolve(m, {{0.001f, cr * 0.55f}, {cr * 0.5f, cr * 0.5f}, {cr * 0.9f, cr * 0.2f}, {cr, -cr * 0.05f}, {cr * 0.9f, -cr * 0.1f}}, 12, 6, MT.mushCap, x, nullptr, R.f());
    addRevolve(m, {{cr * 0.9f, -cr * 0.1f}, {cr * 0.5f, -cr * 0.02f}, {0.004f, -cr * 0.05f}}, 12, 2, MT.mushStem, x, [](float th, float) { return 1.0f + 0.03f * sinf(th * 40); });
  }
  return m;
}
