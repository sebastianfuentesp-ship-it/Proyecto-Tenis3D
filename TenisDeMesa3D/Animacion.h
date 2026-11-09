#pragma once
// ============================================================================
//  Animacion.h  -  Animación procedural del partido (solo GLM, sin OpenGL)
//
//  Qué hace:
//    * Esqueleto de cada jugador (cintura, torso, cabeza, hombro, codo, muñeca,
//      brazo libre) + raqueta agarrada en la mano.
//    * Un PARTIDO completo precalculado (clase Match): saque con lanzamiento de
//      la pelota desde la palma, peloteo de largo variable, y puntos que se
//      PIERDEN de tres formas (no llega a la pelota, red, pelota fuera).
//    * Marcador (puntaje), alternancia del saque cada 2 puntos, reacciones del
//      ganador (celebra) y del perdedor (se lamenta). Al terminar el set el
//      partido se reinicia solo.
//
//  Todo es determinista: el mismo tiempo produce siempre la misma imagen.
//  El azul es la malla del rojo espejada en X, por eso todo se multiplica por
//  'm' = +1 (rojo) / -1 (azul).
// ============================================================================
#include <cmath>
#include <cstdint>
#include <vector>
#include <algorithm>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace Anim
{
    constexpr float PI = 3.14159265f;

    // ---- Parámetros del juego (unidades = metros) --------------------------
    constexpr float kTrackDt     = 1.0f / 120.0f; // paso de la trayectoria precalculada
    constexpr float kHitInterval = 1.0f;          // segundos entre golpes del peloteo
    constexpr float kServeFlight = 1.25f;         // saque: contacto -> golpe del rival
    constexpr float kTableTop    = 0.845f;        // y mundo de la superficie de la mesa
    constexpr float kNetTop      = 0.997f;        // y mundo del borde superior de la red
    constexpr float kBallRadius  = 0.02f;
    constexpr float kGravity     = 6.0f;          // menor que 9.8 => ritmo "cinematográfico"
    constexpr int   kGamePoints  = 11;            // se gana a 11 con 2 de ventaja

    // ---- Articulaciones (mallas del jugador ROJO, espacio local) ----------
    const glm::vec3 kWaist   (0.65f, 0.92f, 0.01f);
    const glm::vec3 kNeck    (0.65f, 1.44f, 0.01f);
    const glm::vec3 kShoulder(0.415f, 1.40f, 0.00f);   // brazo con raqueta
    const glm::vec3 kElbow   (0.29f, 1.10f, 0.05f);
    const glm::vec3 kHand    (0.26f, 0.95f, 0.085f);   // centro de la palma con raqueta
    const glm::vec3 kFreeShoulder(0.885f, 1.40f, 0.00f);
    const glm::vec3 kFreePalm(1.035f, 0.95f, 0.085f);  // palma del brazo libre (sostiene la pelota)
    constexpr float kTorsoAxisX = 0.65f;

    // ---- Raqueta (espacio local de paleta.obj) -----------------------------
    //  hoja hacia -Z, mango hacia +Z, cara roja (de golpeo) en +Y
    const glm::vec3 kRacketGrip  (0.0f, 0.0f,  0.085f);
    const glm::vec3 kRacketBlade (0.0f, 0.0f, -0.085f);

    // ---- Utilidades ---------------------------------------------------------
    inline float smoothstep(float a, float b, float x)
    {
        float t = glm::clamp((x - a) / (b - a), 0.0f, 1.0f);
        return t * t * (3.0f - 2.0f * t);
    }
    inline float q(float t) { return std::round(t / kTrackDt) * kTrackDt; }   // a la rejilla de 1/120 s
    inline glm::vec3 mirrorX(const glm::vec3& v, float m) { return glm::vec3(v.x * m, v.y, v.z); }
    inline glm::mat4 T(const glm::vec3& v) { return glm::translate(glm::mat4(1.0f), v); }
    inline glm::mat4 Rx(float a) { return glm::rotate(glm::mat4(1.0f), a, glm::vec3(1, 0, 0)); }
    inline glm::mat4 Ry(float a) { return glm::rotate(glm::mat4(1.0f), a, glm::vec3(0, 1, 0)); }
    inline glm::mat4 Rz(float a) { return glm::rotate(glm::mat4(1.0f), a, glm::vec3(0, 0, 1)); }
    inline glm::mat4 about(const glm::vec3& pivot, const glm::mat4& r) { return T(pivot) * r * T(-pivot); }

    // ---- Canales de animación (ángulos en el marco del rojo) ---------------
    enum Channel
    {
        C_TWIST, C_PITCH, C_SH_FWD, C_SH_OUT, C_SH_ACROSS, C_ELBOW,
        C_WRIST_FLEX, C_WRIST_ROLL, C_CROUCH, C_LUNGE, C_FREE_ARM, C_FACE,
        C_FREE_ACROSS, C_HEAD_DOWN, C_HOP,
        C_COUNT
    };

    struct Key { float tau; float v[C_COUNT]; };

    //  tau   twist  pitch  shFwd  shOut  shAcr  elbow  wFlex  wRoll  crouch lunge  free   face   fAcr   head   hop

    // Golpe de derecha (tau = 0 es el contacto con la pelota)
    static const Key kDriveKeys[] =
    {
        { -0.60f, { -0.05f, 0.18f, 0.45f, 0.17f, 0.00f, 0.95f, 0.00f, 0.00f, 0.070f, 0.00f, 0.20f,-0.25f, 0.0f, 0.0f, 0.0f } }, // listo
        { -0.32f, { -0.50f, 0.16f, 0.22f, 0.30f,-0.55f, 1.10f,-0.10f, 0.10f, 0.110f,-0.03f, 0.05f,-0.40f, 0.0f, 0.0f, 0.0f } }, // backswing
        { -0.09f, { -0.22f, 0.22f, 0.42f, 0.18f,-0.20f, 0.90f, 0.00f, 0.05f, 0.100f, 0.01f, 0.30f,-0.42f, 0.0f, 0.0f, 0.0f } }, // aceleración
        {  0.00f, { -0.02f, 0.27f, 0.55f, 0.14f, 0.00f, 0.70f, 0.05f, 0.00f, 0.115f, 0.03f, 0.45f,-0.40f, 0.0f, 0.0f, 0.0f } }, // CONTACTO
        {  0.15f, {  0.28f, 0.22f, 0.85f, 0.02f, 0.50f, 0.75f, 0.15f,-0.10f, 0.080f, 0.05f, 0.15f,-0.20f, 0.0f, 0.0f, 0.0f } }, // follow-through
        {  0.34f, {  0.18f, 0.20f, 0.75f, 0.10f, 0.50f, 1.00f, 0.05f,-0.05f, 0.075f, 0.03f, 0.10f,-0.22f, 0.0f, 0.0f, 0.0f } },
        {  0.60f, { -0.05f, 0.18f, 0.45f, 0.17f, 0.00f, 0.95f, 0.00f, 0.00f, 0.070f, 0.00f, 0.20f,-0.25f, 0.0f, 0.0f, 0.0f } }, // listo
    };
    constexpr int kDriveCount = sizeof(kDriveKeys) / sizeof(kDriveKeys[0]);

    // Saque (tau = 0 es el contacto). El brazo libre sostiene la pelota en la palma,
    // la lanza hacia arriba (suelta en tau = -0.55) y la raqueta la golpea al caer.
    static const Key kServeKeys[] =
    {
        { -1.60f, { -0.05f, 0.18f, 0.45f, 0.17f, 0.00f, 0.95f, 0.00f, 0.00f, 0.070f, 0.00f, 0.20f,-0.25f, 0.00f, 0.0f, 0.0f } }, // listo
        { -1.10f, { -0.10f, 0.20f, 0.40f, 0.22f, 0.50f, 1.15f, 0.00f, 0.05f, 0.090f, 0.00f, 1.20f,-0.30f, 1.10f, 0.0f, 0.0f } }, // muestra la pelota
        { -0.62f, { -0.10f, 0.20f, 0.40f, 0.22f, 0.50f, 1.15f, 0.00f, 0.05f, 0.090f, 0.00f, 1.25f,-0.30f, 1.10f, 0.0f, 0.0f } },
        { -0.45f, { -0.12f, 0.20f, 0.38f, 0.22f, 0.45f, 1.15f, 0.00f, 0.05f, 0.090f, 0.00f, 1.55f,-0.30f, 1.10f, 0.0f, 0.0f } }, // acompaña el lanzamiento
        { -0.26f, { -0.30f, 0.17f, 0.25f, 0.30f,-0.05f, 1.20f,-0.10f, 0.10f, 0.105f,-0.02f, 1.60f,-0.45f, 0.90f, 0.0f, 0.0f } }, // backswing
        { -0.08f, { -0.12f, 0.22f, 0.45f, 0.18f, 0.35f, 0.95f, 0.00f, 0.05f, 0.105f, 0.01f, 1.35f,-0.52f, 0.90f, 0.0f, 0.0f } }, // aceleración
        {  0.00f, { -0.02f, 0.25f, 0.55f, 0.14f, 0.50f, 0.75f, 0.05f, 0.00f, 0.110f, 0.02f, 1.20f,-0.55f, 0.85f, 0.0f, 0.0f } }, // CONTACTO
        {  0.20f, {  0.25f, 0.20f, 0.85f, 0.02f, 0.55f, 0.80f, 0.15f,-0.10f, 0.085f, 0.04f, 0.50f,-0.25f, 0.30f, 0.0f, 0.0f } }, // follow-through
        {  0.55f, {  0.05f, 0.18f, 0.55f, 0.15f, 0.10f, 0.95f, 0.00f, 0.00f, 0.070f, 0.00f, 0.25f,-0.25f, 0.05f, 0.0f, 0.0f } },
        {  0.95f, { -0.05f, 0.18f, 0.45f, 0.17f, 0.00f, 0.95f, 0.00f, 0.00f, 0.070f, 0.00f, 0.20f,-0.25f, 0.00f, 0.0f, 0.0f } }, // listo
    };
    constexpr int kServeCount = sizeof(kServeKeys) / sizeof(kServeKeys[0]);

    // Reacciones: el ganador celebra (brazos arriba, salto), el perdedor se lamenta
    static const float kCelebrate[C_COUNT] =
        { -0.10f, -0.05f, 2.55f, 0.30f,-0.20f, 0.45f, 0.00f, 0.00f, 0.020f, 0.00f, 2.35f, -0.25f, 0.15f,-0.25f, 0.045f };
    static const float kSlump[C_COUNT] =
        {  0.00f, 0.40f, 0.18f, 0.12f, 0.00f, 0.35f, 0.00f, 0.00f, 0.030f, 0.00f, 0.05f, -0.25f, 0.00f, 0.50f, 0.0f };

    // Interpolación Hermite con tangentes de diferencias finitas (C1: sin "snaps")
    inline void sampleKeys(const Key* keys, int count, float tau, float out[C_COUNT])
    {
        if (tau <= keys[0].tau || tau >= keys[count - 1].tau)
        {
            const Key& e = (tau <= keys[0].tau) ? keys[0] : keys[count - 1];
            for (int c = 0; c < C_COUNT; ++c) out[c] = e.v[c];
            return;
        }
        int i = 0;
        while (tau > keys[i + 1].tau) ++i;
        const Key& k0 = keys[i];
        const Key& k1 = keys[i + 1];
        const Key& kp = keys[i > 0 ? i - 1 : i];
        const Key& kn = keys[i + 2 < count ? i + 2 : i + 1];
        const float h = k1.tau - k0.tau;
        const float u = (tau - k0.tau) / h;
        const float u2 = u * u, u3 = u2 * u;
        const float h00 = 2 * u3 - 3 * u2 + 1, h10 = u3 - 2 * u2 + u, h01 = -2 * u3 + 3 * u2, h11 = u3 - u2;
        for (int c = 0; c < C_COUNT; ++c)
        {
            float m0 = (k1.v[c] - kp.v[c]) / (k1.tau - kp.tau);
            float m1 = (kn.v[c] - k0.v[c]) / (kn.tau - k0.tau);
            if (i == 0)             m0 = (k1.v[c] - k0.v[c]) / h;
            if (i + 2 >= count)     m1 = (k1.v[c] - k0.v[c]) / h;
            out[c] = h00 * k0.v[c] + h10 * h * m0 + h01 * k1.v[c] + h11 * h * m1;
        }
    }

    // ---- Calendario del partido ---------------------------------------------
    enum StrokeKind { STROKE_DRIVE = 0, STROKE_SERVE = 1, STROKE_MISS = 2 };
    enum MoodKind   { MOOD_CELEBRATE = 0, MOOD_SLUMP = 1 };
    enum EndType    { END_MISSED = 0, END_NET = 1, END_OUT = 2 };

    struct Stroke { float t; int kind; };          // t = instante del contacto (o del swing fallido)
    struct Mood   { float t0, t1; int kind; };

    struct Schedule
    {
        std::vector<Stroke> strokes[2];            // [0] rojo, [1] azul
        std::vector<Mood>   moods[2];
        float period = 60.0f;                      // duración del ciclo completo (partido + pausa)
    };

    inline float moodWeight(const std::vector<Mood>& v, float t, int kind)
    {
        float w = 0.0f;
        for (const Mood& m : v)
        {
            if (m.kind != kind || t < m.t0 || t > m.t1) continue;
            w = std::max(w, smoothstep(m.t0, m.t0 + 0.45f, t) * (1.0f - smoothstep(m.t1 - 0.5f, m.t1, t)));
        }
        return w;
    }

    // ---- Descripción de un jugador y su pose --------------------------------
    struct PlayerSetup
    {
        float m;              // +1 rojo, -1 azul (espejo en X de la malla)
        glm::vec3 basePos;    // posición base en el mundo
        float yawDeg;         // rotación Y de la instancia (-90 rojo, +90 azul)
        glm::vec3 center;     // centro de la malla del cuerpo (Model::GetCenter)
        float minY;           // Model::GetMinY del cuerpo
    };

    struct PlayerPose
    {
        glm::mat4 root;       // mesh-local -> mundo
        glm::mat4 boneLegs, boneTorso, boneFreeArm;   // cuerpo (skinning)
        glm::mat4 boneUpper, boneFore;                // brazo (skinning)
        glm::mat4 headLocal;  // cabeza (sin skinning)  (se multiplica por root)
        glm::mat4 racketLocal;// raqueta                (se multiplica por root)
        float headDown;       // inclinación extra de la cabeza (lamento / alegría)
        float shake;          // 0..1 cuánto niega con la cabeza (derrota)
    };

    // Pose del cuerpo + brazo + raqueta (la cabeza se añade luego, depende de la pelota)
    inline PlayerPose computePose(const PlayerSetup& P, const Schedule& S, float t)
    {
        const float m = P.m;
        const int who = (m > 0.0f) ? 0 : 1;

        // 1) golpe activo (saque, derecha o swing fallido) que rodea al instante t
        float k[C_COUNT];
        int kind = -1;
        float tau = 0.0f;
        {
            float best = 1e9f;
            for (const Stroke& s : S.strokes[who])
            {
                const float d = t - s.t;
                const float lo = (s.kind == STROKE_SERVE) ? -1.6f : -0.65f;
                const float hi = (s.kind == STROKE_SERVE) ?  0.95f :  0.65f;
                if (d >= lo && d <= hi && std::fabs(d) < best)
                {
                    best = std::fabs(d); kind = s.kind; tau = d;
                }
            }
        }
        if (kind == STROKE_SERVE)      sampleKeys(kServeKeys, kServeCount, tau, k);
        else if (kind >= 0)            sampleKeys(kDriveKeys, kDriveCount, tau, k);
        else for (int c = 0; c < C_COUNT; ++c) k[c] = kDriveKeys[0].v[c];

        // 2) reacciones tras el punto
        const float wCel = moodWeight(S.moods[who], t, MOOD_CELEBRATE);
        const float wSlu = moodWeight(S.moods[who], t, MOOD_SLUMP);
        for (int c = 0; c < C_COUNT; ++c)
        {
            k[c] += (kCelebrate[c] - k[c]) * wCel;
            k[c] += (kSlump[c]     - k[c]) * wSlu;
        }

        // 3) vida en reposo: respiración y cambios de apoyo. Las frecuencias son múltiplos
        //    enteros de 2*pi/periodo, así el ciclo del partido cierra sin saltos.
        auto om = [&](float base) {
            return 2.0f * PI * std::max(1.0f, std::round(base * S.period / (2.0f * PI))) / S.period;
        };
        const float ph = (m > 0.0f ? 0.0f : 1.7f);
        k[C_CROUCH] += 0.007f * std::sin(om(2.3f) * t + ph);
        k[C_PITCH]  += 0.018f * std::sin(om(1.6f) * t + ph);
        k[C_SH_OUT] += 0.03f  * std::sin(om(1.9f) * t + ph + 0.8f);

        const glm::vec3 W  = mirrorX(kWaist, m);
        const glm::vec3 N  = mirrorX(kNeck, m);
        const glm::vec3 S_ = mirrorX(kShoulder, m);
        const glm::vec3 E  = mirrorX(kElbow, m);
        const glm::vec3 H  = mirrorX(kHand, m);
        const glm::vec3 FS = mirrorX(kFreeShoulder, m);

        PlayerPose out;

        // Desplazamiento en el mundo: paso adelante + desplazamiento lateral + salto
        const glm::vec3 fwd(-m, 0.0f, 0.0f);                          // rojo mira a -X, azul a +X
        const float side = 0.07f * std::sin(om(0.9f) * t + ph) * (1.0f - 0.6f * (wCel + wSlu));
        const float hop  = k[C_HOP] * std::fabs(std::sin(7.5f * t));
        const glm::vec3 pos = P.basePos + fwd * k[C_LUNGE] + glm::vec3(0.0f, hop, side);

        glm::mat4 root(1.0f);
        root = glm::translate(root, pos);
        root = glm::rotate(root, glm::radians(P.yawDeg), glm::vec3(0, 1, 0));
        root = glm::translate(root, glm::vec3(-P.center.x, -P.minY, -P.center.z));
        out.root = root;

        // Piernas se comprimen (rodillas flexionadas) y el torso baja con ellas
        const float sy = 1.0f - k[C_CROUCH] / kWaist.y;
        out.boneLegs = glm::scale(glm::mat4(1.0f), glm::vec3(1.0f, sy, 1.0f));

        out.boneTorso = T(glm::vec3(0, -k[C_CROUCH], 0)) *
                        about(W, Ry(m * k[C_TWIST]) * Rx(k[C_PITCH]));

        // Brazo libre: se eleva hacia delante (sostiene / lanza la pelota) y se cierra hacia el centro
        out.boneFreeArm = out.boneTorso * about(FS, Ry(-m * k[C_FREE_ACROSS]) * Rx(-k[C_FREE_ARM]));

        // Brazo con raqueta: hombro (adelante, afuera, a través del cuerpo) y codo
        const glm::mat4 rSh = Ry(m * k[C_SH_ACROSS]) * Rx(-k[C_SH_FWD]) * Rz(m * -k[C_SH_OUT]);
        out.boneUpper = out.boneTorso * about(S_, rSh);
        out.boneFore  = out.boneUpper * about(E, Rx(-k[C_ELBOW]));

        // Raqueta: agarrada en la palma; muñeca = flexión + giro del antebrazo
        const glm::mat4 rWrist = Rx(k[C_WRIST_FLEX]) * Ry(m * k[C_WRIST_ROLL]);
        const glm::mat4 rGrip  = Rx(PI) * Rx(k[C_FACE]);
        out.racketLocal = out.boneFore * T(H) * rWrist * rGrip * T(-kRacketGrip);

        out.headLocal = out.boneTorso;
        out.headDown = k[C_HEAD_DOWN];
        out.shake = wSlu;
        return out;
    }

    // Cabeza: mira la pelota con ganancia suave; el perdedor niega con la cabeza
    inline void applyHead(PlayerPose& pose, const PlayerSetup& P, const glm::vec3& ballWorld, float t)
    {
        const float m = P.m;
        const glm::vec3 N = mirrorX(kNeck, m);
        const glm::mat4 toLocal = glm::inverse(pose.root * pose.boneTorso);
        glm::vec3 d = glm::vec3(toLocal * glm::vec4(ballWorld, 1.0f)) - N;
        float yaw = std::atan2(d.x * m, std::max(d.z, 0.05f));
        float pitchDown = std::atan2(-d.y, std::sqrt(d.x * d.x + d.z * d.z));
        yaw = glm::clamp(yaw * 0.75f, -0.9f, 0.9f);
        pitchDown = glm::clamp(pitchDown * 0.55f, -0.35f, 0.55f);
        yaw = yaw * (1.0f - pose.shake) + pose.shake * 0.45f * std::sin(6.5f * t);
        pitchDown += pose.headDown;
        pose.headLocal = pose.boneTorso * about(N, Ry(m * yaw) * Rx(pitchDown));
    }

    // Punto donde la bola toca la cara de la raqueta y normal de la cara (mundo)
    inline glm::vec3 racketContact(const PlayerPose& pose, glm::vec3* normalOut = nullptr)
    {
        const glm::mat4 M = pose.root * pose.racketLocal;
        const glm::vec3 blade = glm::vec3(M * glm::vec4(kRacketBlade, 1.0f));
        const glm::vec3 n = glm::normalize(glm::vec3(M * glm::vec4(0, 1, 0, 0)));
        if (normalOut) *normalOut = n;
        return blade + n * (0.014f + kBallRadius + 0.002f);
    }

    // Palma del brazo libre (mundo) con la pelota apoyada encima
    inline glm::vec3 freePalmBall(const PlayerSetup& P, const PlayerPose& pose)
    {
        const glm::vec3 pl = mirrorX(kFreePalm, P.m);
        const glm::vec3 w = glm::vec3(pose.root * pose.boneFreeArm * glm::vec4(pl, 1.0f));
        return w + glm::vec3(0.0f, 0.045f, 0.0f);
    }

    // ---- Parábola entre dos puntos en un tiempo D ----------------------------
    inline glm::vec3 parabola(const glm::vec3& a, const glm::vec3& b, float D, float tt)
    {
        const float u = tt / D;
        const float vy0 = (b.y - a.y) / D + 0.5f * kGravity * D;
        glm::vec3 p = glm::mix(a, b, u);
        p.y = a.y + vy0 * tt - 0.5f * kGravity * tt * tt;
        return p;
    }
    inline glm::vec3 parabolaEndVelocity(const glm::vec3& a, const glm::vec3& b, float D)
    {
        return glm::vec3((b.x - a.x) / D, (b.y - a.y) / D - 0.5f * kGravity * D, (b.z - a.z) / D);
    }

    // =========================================================================
    //  Match: partido completo precalculado
    // =========================================================================
    struct ScoreEvent { float t; int rojo, azul; int scorer; };   // scorer: 0 rojo, 1 azul, -1 ninguno

    class Match
    {
    public:
        Schedule sch;
        PlayerSetup setup[2];
        std::vector<glm::vec4> track;        // (x, y, z, escala) cada kTrackDt
        std::vector<ScoreEvent> scores;
        int pointsPlayed = 0;
        struct PointInfo { int server, loser, end, L; float tS, tL, tEnd; };
        std::vector<PointInfo> info;              // resumen de cada punto (depuración / UI)

        float period() const { return sch.period; }
        float localTime(float t) const { return std::fmod(std::max(t, 0.0f), sch.period); }

        glm::vec4 ball(float tl) const
        {
            const float f = tl / kTrackDt;
            int i = (int)std::floor(f);
            i = glm::clamp(i, 0, (int)track.size() - 2);
            const float u = glm::clamp(f - (float)i, 0.0f, 1.0f);
            return glm::mix(track[i], track[i + 1], u);
        }

        ScoreEvent scoreAt(float tl) const
        {
            ScoreEvent cur = { -100.0f, 0, 0, -1 };
            for (const ScoreEvent& e : scores) if (e.t <= tl) cur = e;
            return cur;
        }

        // Pose completa (cuerpo + cabeza mirando la pelota)
        PlayerPose pose(int who, float tl) const
        {
            PlayerPose p = computePose(setup[who], sch, tl);
            const glm::vec4 b = ball(tl);
            const glm::vec3 look = (b.w > 0.05f) ? glm::vec3(b) : glm::vec3(0.0f, 1.0f, 0.0f);
            applyHead(p, setup[who], look, tl);
            return p;
        }

        void build(const PlayerSetup& rojo, const PlayerSetup& azul);

    private:
        glm::vec3 contactAt(int who, float t) const
        {
            return racketContact(computePose(setup[who], sch, t));
        }
    };

    inline void Match::build(const PlayerSetup& rojo, const PlayerSetup& azul)
    {
        setup[0] = rojo; setup[1] = azul;
        sch = Schedule();
        scores.clear();

        // ---------------- 1) CALENDARIO: quién gana cada punto y cómo ----------------
        struct Plan { int server, loser, end, L; float tS, tL, tEnd, tMiss; };
        std::vector<Plan> plans;

        uint32_t rng = 0x2468ACEu;
        auto rnd = [&]() { rng = rng * 1664525u + 1013904223u; return (float)(rng >> 8) * (1.0f / 16777216.0f); };

        int r = 0, a = 0, n = 0;
        float tS = q(2.8f);                                  // primer saque
        auto over = [&]() { return (r >= kGamePoints || a >= kGamePoints) && std::abs(r - a) >= 2; };

        while (!over() && n < 60)
        {
            // El saque cambia cada 2 puntos (cada punto desde 10-10)
            const int server = (n < 2 * (kGamePoints - 1)) ? ((n / 2) % 2) : (n % 2);
            const int loser = (rnd() < 0.5f) ? 0 : 1;

            // El perdedor es quien debería golpear el golpe número L.
            // L par -> le toca al sacador; L impar -> al receptor.
            int L;
            if (loser == server) { static const int E[] = { 2, 4, 2, 6 }; L = E[(int)(rnd() * 4.0f) & 3]; }
            else                 { static const int O[] = { 1, 3, 1, 5 }; L = O[(int)(rnd() * 4.0f) & 3]; }

            const float u = rnd();
            const int end = (u < 0.40f) ? END_MISSED : ((u < 0.70f) ? END_NET : END_OUT);

            // Golpes exitosos: 0 = saque, 1..L-1 = devoluciones
            sch.strokes[server].push_back({ tS, STROKE_SERVE });
            const float t1 = q(tS + kServeFlight);
            for (int i = 1; i <= L - 1; ++i)
            {
                const int hitter = (i % 2 == 0) ? server : 1 - server;
                sch.strokes[hitter].push_back({ q(t1 + (i - 1) * kHitInterval), STROKE_DRIVE });
            }

            // Golpe L (el del error)
            const float tL = q(t1 + (L - 1) * kHitInterval);
            float tMiss = 0.0f, tEnd;
            if (end == END_MISSED)
            {
                tMiss = q(tL + 0.18f);                      // el swing llega tarde
                sch.strokes[loser].push_back({ tMiss, STROKE_MISS });
                tEnd = q(tL + 0.70f);
            }
            else if (end == END_NET)
            {
                sch.strokes[loser].push_back({ tL, STROKE_DRIVE });
                tEnd = q(tL + 0.35f + 0.40f);
            }
            else
            {
                sch.strokes[loser].push_back({ tL, STROKE_DRIVE });
                tEnd = q(tL + 1.00f);
            }

            plans.push_back({ server, loser, end, L, tS, tL, tEnd, tMiss });

            const int winner = 1 - loser;
            if (winner == 0) ++r; else ++a;
            scores.push_back({ tEnd, r, a, winner });

            sch.moods[winner].push_back({ tEnd + 0.20f, tEnd + 2.00f, MOOD_CELEBRATE });
            sch.moods[loser ].push_back({ tEnd + 0.20f, tEnd + 2.00f, MOOD_SLUMP });

            tS = q(tEnd + 3.2f);
            ++n;
        }
        pointsPlayed = (int)plans.size();
        info.clear();
        for (const Plan& p : plans) info.push_back({ p.server, p.loser, p.end, p.L, p.tS, p.tL, p.tEnd });
        sch.period = q(plans.back().tEnd + 4.6f);

        // ---------------- 2) TRAYECTORIA DE LA PELOTA (precalculada) ----------------
        const int N = (int)(sch.period / kTrackDt) + 3;
        track.assign(N, glm::vec4(0.0f, 1.0f, 0.0f, 0.0f));

        auto idx = [&](float t) { return (int)std::lround(t / kTrackDt); };
        auto put = [&](int k, const glm::vec3& p, float s) { if (k >= 0 && k < N) track[k] = glm::vec4(p, s); };
        auto arc = [&](float t0, const glm::vec3& A, const glm::vec3& B, float D)
        {
            const int k0 = idx(t0), k1 = idx(t0 + D);
            for (int k = k0; k < k1; ++k) put(k, parabola(A, B, D, k * kTrackDt - t0), 1.0f);
        };

        // Integrador para lo que pasa DESPUÉS del último golpe: rebota en mesa, red y suelo
        auto tail = [&](float t0, glm::vec3 p, glm::vec3 v, float tVanish)
        {
            const float h = kTrackDt / 4.0f;
            const float R = kBallRadius;
            int k = idx(t0);
            for (;; ++k)
            {
                const float tt = k * kTrackDt;
                if (tt > tVanish + 0.35f || k >= N) break;
                const float s = 1.0f - smoothstep(tVanish, tVanish + 0.3f, tt);
                put(k, p, s);
                for (int sstep = 0; sstep < 4; ++sstep)
                {
                    const glm::vec3 prev = p;
                    v.y -= kGravity * h;
                    p += v * h;
                    // red
                    if (prev.x * p.x <= 0.0f && std::fabs(p.z) < 0.92f && p.y < kNetTop + R && p.y > kTableTop - 0.05f && std::fabs(prev.x - p.x) > 1e-7f)
                    {
                        p.x = (prev.x >= 0.0f ? 1.0f : -1.0f) * 0.006f;
                        v.x *= -0.12f; v.z *= 0.4f;
                    }
                    // mesa
                    if (std::fabs(p.x) < 1.37f && std::fabs(p.z) < 0.762f && v.y < 0.0f &&
                        p.y <= kTableTop + R && prev.y > kTableTop + R - 0.15f)
                    {
                        p.y = kTableTop + R; v.y = -v.y * 0.86f; v.x *= 0.97f; v.z *= 0.97f;
                    }
                    // suelo
                    if (p.y <= R && v.y < 0.0f)
                    {
                        p.y = R; v.y = -v.y * 0.55f; v.x *= 0.85f; v.z *= 0.85f;
                        if (std::fabs(v.y) < 0.25f) v.y = 0.0f;
                    }
                    if (p.y <= R + 1e-4f && v.y == 0.0f) { v.x *= 0.998f; v.z *= 0.998f; }
                }
            }
        };

        // Tramo de peloteo normal: golpe -> rebote en la mitad del rival -> golpe
        auto rally = [&](float t0, const glm::vec3& A, const glm::vec3& C) -> glm::vec3 /* vel. final */
        {
            const float D1 = q(0.625f), D2 = q(kHitInterval - 0.625f);
            glm::vec3 B = glm::mix(A, C, 0.625f);
            B.y = kTableTop + kBallRadius;
            arc(t0, A, B, D1);
            arc(t0 + D1, B, C, D2);
            return parabolaEndVelocity(B, C, D2);
        };

        // Saque: golpe -> rebote en SU mitad -> rebote en la del rival -> golpe del rival
        auto serveFlight = [&](float t0, const glm::vec3& A, const glm::vec3& C) -> glm::vec3
        {
            const float sA = (A.x >= 0.0f) ? 1.0f : -1.0f;
            const float D1 = q(0.30f), D2 = q(0.55f), D3 = q(0.40f);
            glm::vec3 B1(sA * 0.90f, kTableTop + kBallRadius, glm::mix(A.z, C.z, 0.25f));
            glm::vec3 B2(-sA * 0.80f, kTableTop + kBallRadius, glm::mix(A.z, C.z, 0.65f));
            arc(t0, A, B1, D1);
            arc(t0 + D1, B1, B2, D2);
            arc(t0 + D1 + D2, B2, C, D3);
            return parabolaEndVelocity(B2, C, D3);
        };

        for (const Plan& pl : plans)
        {
            const PlayerSetup& S = setup[pl.server];

            // golpe i (0 = saque) -> quién lo da y cuándo
            auto hitterOf = [&](int i) { return (i % 2 == 0) ? pl.server : 1 - pl.server; };
            auto timeOf = [&](int i) { return (i == 0) ? pl.tS : q(pl.tS + kServeFlight + (i - 1) * kHitInterval); };

            std::vector<glm::vec3> C(pl.L + 1);
            for (int i = 0; i <= pl.L; ++i)
            {
                float ti = timeOf(i);
                if (i == pl.L && pl.end == END_MISSED) ti = pl.tMiss;
                C[i] = contactAt(hitterOf(i), ti);
            }

            // Pelota de saque en la palma (aparece con un pequeño "pop") y lanzamiento
            const float tH = q(pl.tS - 1.15f), tRel = q(pl.tS - 0.55f);
            for (int k = idx(tH); k < idx(tRel); ++k)
            {
                const float tt = k * kTrackDt;
                const PlayerPose pp = computePose(S, sch, tt);
                put(k, freePalmBall(S, pp), smoothstep(tH, tH + 0.25f, tt));
            }
            const glm::vec3 P0 = freePalmBall(S, computePose(S, sch, tRel));
            arc(tRel, P0, C[0], q(0.55f));

            // Punto de "no llego": la pelota pasa lejos de la raqueta del perdedor
            glm::vec3 missPoint(0.0f);
            if (pl.end == END_MISSED)
            {
                const float dir = (setup[pl.loser].m > 0.0f) ? 1.0f : -1.0f;      // hacia su espalda
                missPoint = C[pl.L] + glm::vec3(dir * 0.15f, 0.0f, -0.55f);
            }
            auto dest = [&](int i) { return (i == pl.L && pl.end == END_MISSED) ? missPoint : C[i]; };

            // Saque y peloteo hasta el golpe L
            glm::vec3 vEnd = serveFlight(pl.tS, C[0], dest(1));
            for (int i = 1; i <= pl.L - 1; ++i)
                vEnd = rally(timeOf(i), C[i], dest(i + 1));

            // Qué pasa con la pelota al final del punto
            const float tVanish = pl.tEnd + 1.5f;
            if (pl.end == END_MISSED)
            {
                tail(pl.tL, missPoint, vEnd, tVanish);
            }
            else if (pl.end == END_NET)
            {
                const float sH = (C[pl.L].x >= 0.0f) ? 1.0f : -1.0f;
                const glm::vec3 Np(sH * 0.02f, 0.93f, C[pl.L].z);
                const float Dn = q(0.35f);
                arc(pl.tL, C[pl.L], Np, Dn);
                tail(pl.tL + Dn, Np, parabolaEndVelocity(C[pl.L], Np, Dn), tVanish);
            }
            else // END_OUT: pelota ancha, pasa por el costado de la mesa y cae al suelo
            {
                const float sH = (C[pl.L].x >= 0.0f) ? 1.0f : -1.0f;
                const glm::vec3 Fp(-sH * 1.00f, kBallRadius, 1.45f);
                const float Do = q(1.00f);
                arc(pl.tL, C[pl.L], Fp, Do);
                tail(pl.tL + Do, Fp, parabolaEndVelocity(C[pl.L], Fp, Do), tVanish);
            }
        }
    }
}
