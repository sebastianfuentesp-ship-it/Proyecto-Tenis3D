#pragma once
// ============================================================================
//  Keyframes.h  -  Animación por KEYFRAMES de la Escena (Animación + Luces)
//  Guarda el tiempo del partido de tenis y el estado de iluminación.
// ============================================================================
#include <cmath>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <glm/glm.hpp>

// Estado guardado en un keyframe de la animación
struct AnimKeyframe
{
    float animTime = 0.0f;     // Instante de tiempo en la línea del partido (jugadores, pelota, marcador)
    int lightMode = 1;         // Luces del techo (0: Apagado, 1: Cálido, 2: Azul, 3: Rojo, 4: Reflector Cónico)
    int spotOn = 0;            // Reflector cónico adicional (0/1)
    int dirOn = 0;             // Luz direccional (0/1)
    float duration = 3.0f;     // Segundos de transición hacia este keyframe
};

class KeyframeAnimator
{
public:
    static constexpr float kDefaultSegment = 3.0f;   // Duración por defecto entre keyframes

    std::vector<AnimKeyframe> frames;
    float speed = 1.0f;

    static AnimKeyframe Capture(float animTime, int lightMode, bool spotOn, bool dirOn)
    {
        AnimKeyframe k;
        k.animTime = animTime;
        k.lightMode = lightMode;
        k.spotOn = spotOn ? 1 : 0;
        k.dirOn = dirOn ? 1 : 0;
        return k;
    }

    void Add(AnimKeyframe k)
    {
        k.duration = frames.empty() ? 0.0f : kDefaultSegment;
        frames.push_back(k);
    }

    bool RemoveLast()
    {
        if (frames.empty()) return false;
        frames.pop_back();
        return true;
    }

    void Clear() { frames.clear(); }
    int Count() const { return (int)frames.size(); }

    // ---------------- Archivo de texto ----------------
    bool Save(const std::string& file) const
    {
        std::ofstream out(file.c_str());
        if (!out) return false;
        out << "# Keyframes de animacion y luces: animTime lightMode spotOn dirOn duration\n";
        for (const AnimKeyframe& k : frames)
        {
            out << k.animTime << ' ' << k.lightMode << ' '
                << k.spotOn << ' ' << k.dirOn << ' ' << k.duration << '\n';
        }
        return (bool)out;
    }

    bool Load(const std::string& file)
    {
        std::ifstream in(file.c_str());
        if (!in) return false;
        std::vector<AnimKeyframe> loaded;
        std::string line;
        while (std::getline(in, line))
        {
            if (line.empty() || line[0] == '#') continue;
            std::istringstream ss(line);
            AnimKeyframe k;
            if (ss >> k.animTime >> k.lightMode >> k.spotOn >> k.dirOn >> k.duration)
            {
                k.lightMode = glm::clamp(k.lightMode, 0, 4);
                k.spotOn = k.spotOn ? 1 : 0;
                k.dirOn = k.dirOn ? 1 : 0;
                k.duration = std::max(k.duration, 0.2f);
                loaded.push_back(k);
            }
        }
        if (loaded.empty()) return false;
        loaded[0].duration = 0.0f;
        frames = loaded;
        return true;
    }

    // ---------------- Reproducción ----------------
    bool Play(const AnimKeyframe& current)
    {
        if (frames.empty()) return false;
        path.clear();

        AnimKeyframe c = current;
        c.duration = 0.0f;
        path.push_back(c);

        for (size_t i = 0; i < frames.size(); ++i)
        {
            AnimKeyframe f = frames[i];
            if (i == 0) f.duration = 1.5f;
            path.push_back(f);
        }
        time = 0.0f;
        playing = true;
        return true;
    }

    void Stop() { playing = false; }
    bool IsPlaying() const { return playing; }
    float Time() const { return time; }

    float TotalDuration() const
    {
        float t = 0.0f;
        for (size_t i = 1; i < path.size(); ++i) t += path[i].duration;
        return t;
    }

    float Progress() const
    {
        const float total = TotalDuration();
        return total > 1e-6f ? glm::clamp(time / total, 0.0f, 1.0f) : 1.0f;
    }

    AnimKeyframe Update(float dt)
    {
        const float total = TotalDuration();
        time += dt * speed;
        if (time >= total)
        {
            time = total;
            playing = false;
        }
        return Evaluate(time);
    }

    AnimKeyframe Evaluate(float t) const
    {
        const int n = (int)path.size();
        if (n == 0) return AnimKeyframe();
        if (n == 1) return path[0];

        t = glm::clamp(t, 0.0f, TotalDuration());

        int i = 0;
        float acc = 0.0f;
        while (i < n - 2 && t >= acc + path[i + 1].duration)
        {
            acc += path[i + 1].duration;
            ++i;
        }
        const float dur = path[i + 1].duration;
        const float u = dur > 1e-6f ? glm::clamp((t - acc) / dur, 0.0f, 1.0f) : 1.0f;

        const AnimKeyframe& b = path[i];
        const AnimKeyframe& c = path[i + 1];

        AnimKeyframe out;
        // Interpolación fluida del avance de tiempo del partido
        out.animTime = glm::mix(b.animTime, c.animTime, u);

        const AnimKeyframe& st = (t >= TotalDuration()) ? path[n - 1] : b;
        out.lightMode = st.lightMode;
        out.spotOn = st.spotOn;
        out.dirOn = st.dirOn;
        out.duration = 0.0f;
        return out;
    }

private:
    std::vector<AnimKeyframe> path;
    float time = 0.0f;
    bool playing = false;
};