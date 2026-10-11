#include "game/startup/startupscene.h"

void startupscene::Scene::reset() {
    *this = Scene{};
}

void startupscene::Scene::enterTitle() {
    reset();
    boot = false;
    soundRequest = 0x80000021;
}

void startupscene::Scene::update(bool audioFinished, bool startPressed, bool titleInitializationFinished) {
    if (request != -1) return;
    if (boot) {
        if (bootState == 0) {
            bootState = 1;
        } else if (bootState == 1) {
            if (audioFinished) bootState = 2;
        } else {
            boot = false;
            soundRequest = 0x80000021;
        }
        return;
    }
    if (titlePhase == 0) {
        titlePhase = 1;
    } else if (titlePhase == 1) {
        updateTitle(startPressed);
        if (titleInitializationFinished) titlePhase = 2;
    } else {
        if (startPressed) startLatched = true;
        if (!startLatched || wait > 0) {
            updateTitle(startPressed);
            if (ready && wait != 0) --wait;
        } else {
            request = 3;
            soundRequest = 0x3802;
        }
    }
    if (--timeout == 0) request = 4;
}

void startupscene::Scene::updateTitle(bool startPressed) {
    if (startPressed && state < 2) {
        state = 2;
        backgroundAlpha = 255;
        logoAlpha = 255;
        copyrightAlpha = 255;
        fade = 240;
    }
    switch (state) {
    case 0:
        if (++fade < 255) {
            logoVisible = false;
            copyrightVisible = false;
            startVisible = false;
            backgroundAlpha = static_cast<unsigned>(fade);
        } else {
            state = 1;
            fade = 0;
            backgroundAlpha = 255;
            logoAlpha = 0;
        }
        break;
    case 1:
        fade += 4;
        if (fade < 255) {
            logoVisible = true;
            copyrightVisible = true;
            logoAlpha = static_cast<unsigned>(fade);
            copyrightAlpha = static_cast<unsigned>(fade);
            startVisible = false;
        } else {
            state = 2;
            logoAlpha = 255;
            copyrightAlpha = 255;
            fade = 0;
        }
        break;
    case 2:
    case 4:
        if (++fade > 254) state = 3;
        logoVisible = true;
        copyrightVisible = true;
        startVisible = true;
        startAlpha = static_cast<unsigned>(fade) & 255;
        break;
    case 3:
        if (--fade < 1) state = 4;
        logoVisible = true;
        copyrightVisible = true;
        startVisible = true;
        startAlpha = static_cast<unsigned>(fade) & 255;
        break;
    }
    if (state > 1 && state < 5) {
        flash1 = (flash1 + 1) % 330;
        flash2 = (flash2 + 1) % 430;
    } else {
        flash1 = 0;
        flash2 = 0;
    }
    ready = state >= 2;
}

std::vector<startupscene::Picture> startupscene::Scene::pictures(const startupresources::Resources& resources) const {
    if (boot) return {{0, 0, 0, 640, 480, 1, {0, 0, 0, 1}, {220.0f / 255, 0, 0, 1}}};
    std::vector<Picture> result;
    for (const auto& pane : resources.panes) {
        bool visible = pane.visible;
        unsigned alpha = pane.alpha;
        Picture picture{pane.texture, static_cast<float>(pane.x), static_cast<float>(pane.y),
            static_cast<float>(pane.width), static_cast<float>(pane.height)};
        if (pane.tag == "yski") alpha = backgroundAlpha;
        if (pane.tag == "logo") {
            visible = logoVisible;
            alpha = logoAlpha;
            if (state == 1 && resources.sine.size() == 4096) {
                const auto height = static_cast<int>(pane.height * resources.sine[(static_cast<unsigned>(fade) & 1023) * 4]);
                picture.y = static_cast<float>(pane.y + pane.height - height);
                picture.height = static_cast<float>(height);
            }
        }
        if (pane.tag == "copy") {
            visible = copyrightVisible;
            alpha = copyrightAlpha;
        }
        if (pane.tag == "strt") {
            visible = startVisible;
            alpha = startAlpha;
            picture.white = {1, 0, 0, 1};
        }
        if (pane.tag == "mpc1") visible = state > 1 && state < 5 && !(flash1 < 300 || (flash1 > 304 && flash1 < 325));
        if (pane.tag == "mpc2") visible = state > 1 && state < 5 && !(flash2 < 400 || (flash2 > 404 && flash2 < 425));
        picture.alpha = alpha / 255.0f;
        if (visible && picture.height > 0) result.push_back(picture);
    }
    return result;
}

bool startupscene::Scene::isBoot() const { return boot; }
int startupscene::Scene::requestedScene() const { return request; }
unsigned startupscene::Scene::titleState() const { return state; }
unsigned startupscene::Scene::takeSoundRequest() {
    const auto result = soundRequest;
    soundRequest = 0;
    return result;
}
