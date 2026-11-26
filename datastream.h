#pragma once

#include <deque>
#include <optional>
#include <vector>

namespace DataStream {

// UTC Unix seconds. Azimuth is unwrapped to preserve the intended direction.
struct Point {
    double time;
    double azimuth;
    double elevation;
};

struct Limits {
    double minAzimuth, maxAzimuth, minElevation, maxElevation;
    double azimuthRate, elevationRate, lookahead;
};

enum class Command { Start, Target, End, Abort, Status };
enum class Mode { Idle, Active, Ending };

struct Reply {
    bool accepted;
    int free;
    Mode mode;
};

class Backend {
public:
    virtual ~Backend() = default;
    virtual bool connected() const = 0;
    virtual int capacity() const = 0;
    virtual void advance(double now) = 0;
    virtual bool send(Command command, std::optional<Point> point) = 0;
    virtual std::optional<Reply> receive() = 0;
};

class Simulator final : public Backend {
public:
    explicit Simulator(int capacity, double delay = 0);
    bool connected() const override { return online; }
    int capacity() const override { return capacity_; }
    void advance(double now) override;
    bool send(Command command, std::optional<Point> point) override;
    std::optional<Reply> receive() override;
    std::optional<Point> pointing() const;

    bool online = true;
    bool rejectNext = false;
    bool dropNextReply = false;
    bool corruptNextReply = false;

private:
    int capacity_;
    double delay_;
    double now_ = 0;
    double readyAt_ = 0;
    Mode mode_ = Mode::Idle;
    std::deque<Point> queue_;
    std::optional<Point> previous_;
    std::optional<Reply> reply_;
};

class Session {
public:
    enum class State { Idle, Streaming, Draining, Completed, Aborted, Fault };

    Session(Backend &backend, Limits limits);
    bool start(std::vector<Point> trajectory, double now);
    void tick(double now);
    void abort();
    State state() const { return state_; }
    const char *error() const { return error_; }

private:
    bool issue(Command command, std::optional<Point> point = {});
    void fault(const char *error);
    bool active() const;

    Backend &backend_;
    Limits limits_;
    State state_ = State::Idle;
    const char *error_ = "";
    std::vector<Point> trajectory_;
    std::size_t next_ = 0;
    std::optional<Command> pending_;
    bool abortRequested_ = false;
    double now_ = 0;
    double sentAt_ = 0;
};

}
