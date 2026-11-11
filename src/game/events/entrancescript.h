#pragma once

#include <cstdint>
#include <array>
#include <string>
#include <string_view>

namespace gameevent {
    enum class ActorOperation {
        Animation, Look, Path, Stop, Start, Sound, Position, CollisionOn, CollisionOff,
        Alpha, FixedAlpha, ShadowOn, ShadowOff, BodyOn, BodyOff, MaskOn, MaskOff, DisplayOn, DisplayOff
    };

    struct ActorCommand {
        ActorOperation operation;
        std::string_view argument;
        std::array<float, 3> position{};
        unsigned value = 0;
    };

    class EntranceServices {
    public:
        virtual ~EntranceServices() = default;
        virtual bool flag(unsigned index) const = 0;
        virtual void menu(bool enabled) = 0;
        virtual void stopPlayer() = 0;
        virtual void releasePlayer() = 0;
        virtual void music(unsigned index) = 0;
        virtual void camera(std::string_view resource) = 0;
        virtual void lighting(std::string_view resource) = 0;
        virtual void shadowOff(std::string_view actor) = 0;
        virtual void endCamera() = 0;
        virtual void closeMessage() = 0;
        virtual bool messageClosed() const = 0;
        virtual void finish() = 0;
        virtual bool actorExists(std::string_view actor) const;
        virtual void actorCommand(std::string_view actor, const ActorCommand& command);
        virtual void setFlag(unsigned index);
        virtual void sound(unsigned index);
        virtual void effect(std::string_view placement, std::string_view resource);
        virtual void fullscreen(bool enabled);
        virtual void playerEvent(unsigned event);
        virtual void releaseActor();
        virtual void blendCamera();
        virtual void speak(unsigned window, unsigned line);
        virtual void messageColor(unsigned window, int color);
        virtual void closeWindow(unsigned window);
    };

    class EntranceScript {
    public:
        explicit EntranceScript(std::string source);
        bool update(EntranceServices& services);
        bool finished() const;

    private:
        std::string source;
        std::size_t cursor = 0;
        std::uint32_t wait = 0;
        unsigned state = 1;
        std::uint8_t messageWindow = 0;
        bool completed = false;
        std::string_view token(char begin, char end);
        unsigned number();
        std::int32_t integer();
        void actor(EntranceServices& services);
    };
}
