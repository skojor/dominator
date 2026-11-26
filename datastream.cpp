#include "datastream.h"

#include <cmath>
#include <utility>

namespace DataStream {

Simulator::Simulator(int capacity, double delay)
    : capacity_(capacity), delay_(delay)
{
}

void Simulator::advance(double now)
{
    now_ = now;
    while (!queue_.empty() && queue_.front().time <= now) {
        previous_ = queue_.front();
        queue_.pop_front();
    }
    if (mode_ == Mode::Ending && queue_.empty())
        mode_ = Mode::Idle;
}

bool Simulator::send(Command command, std::optional<Point> point)
{
    if (!online || reply_)
        return false;

    bool accepted = !std::exchange(rejectNext, false);
    if (accepted) {
        switch (command) {
        case Command::Start:
            accepted = mode_ == Mode::Idle;
            if (accepted) {
                previous_.reset();
                mode_ = Mode::Active;
            }
            break;
        case Command::Target:
            accepted = mode_ == Mode::Active && point && point->time > now_
                       && static_cast<int>(queue_.size()) < capacity_
                       && (queue_.empty() || point->time > queue_.back().time);
            if (accepted)
                queue_.push_back(*point);
            break;
        case Command::End:
            accepted = mode_ == Mode::Active;
            if (accepted)
                mode_ = queue_.empty() ? Mode::Idle : Mode::Ending;
            break;
        case Command::Abort:
            queue_.clear();
            previous_.reset();
            mode_ = Mode::Idle;
            break;
        case Command::Status:
            break;
        }
    }

    if (!std::exchange(dropNextReply, false)) {
        const int free = std::exchange(corruptNextReply, false)
            ? -1 : capacity_ - static_cast<int>(queue_.size());
        reply_ = Reply{accepted, free, mode_};
        readyAt_ = now_ + delay_;
    }
    return true;
}

std::optional<Reply> Simulator::receive()
{
    if (!online || now_ < readyAt_)
        return {};
    return std::exchange(reply_, {});
}

std::optional<Point> Simulator::pointing() const
{
    if (!previous_ || queue_.empty())
        return previous_;
    const auto &a = *previous_;
    const auto &b = queue_.front();
    const double fraction = (now_ - a.time) / (b.time - a.time);
    return Point{now_, a.azimuth + fraction * (b.azimuth - a.azimuth),
                 a.elevation + fraction * (b.elevation - a.elevation)};
}

Session::Session(Backend &backend, Limits limits)
    : backend_(backend), limits_(limits)
{
}

bool Session::active() const
{
    return state_ == State::Streaming || state_ == State::Draining;
}

void Session::fault(const char *error)
{
    state_ = State::Fault;
    error_ = error;
}

bool Session::issue(Command command, std::optional<Point> point)
{
    if (!backend_.send(command, point)) {
        fault("Could not send request; backend state unknown.");
        return false;
    }
    pending_ = command;
    sentAt_ = now_;
    return true;
}

bool Session::start(std::vector<Point> trajectory, double now)
{
    if (active() || state_ == State::Fault)
        return false;
    if (!backend_.connected() || backend_.capacity() < 1 || trajectory.empty()
        || !std::isfinite(now) || !std::isfinite(limits_.lookahead)
        || !(limits_.lookahead > 0 && limits_.azimuthRate > 0 && limits_.elevationRate > 0)) {
        fault("Invalid session configuration or unavailable backend.");
        return false;
    }
    const Point *previous = nullptr;
    for (const auto &point : trajectory) {
        if (!std::isfinite(point.time) || point.time <= now
            || !(point.azimuth >= limits_.minAzimuth && point.azimuth <= limits_.maxAzimuth
                 && point.elevation >= limits_.minElevation && point.elevation <= limits_.maxElevation)
            || (previous && (point.time <= previous->time
                || point.time - previous->time > limits_.lookahead
                || std::abs(point.azimuth - previous->azimuth) / (point.time - previous->time) > limits_.azimuthRate
                || std::abs(point.elevation - previous->elevation) / (point.time - previous->time) > limits_.elevationRate))) {
            fault("Invalid, stale, or infeasible trajectory.");
            return false;
        }
        previous = &point;
    }
    trajectory_ = std::move(trajectory);
    next_ = 0;
    abortRequested_ = false;
    now_ = now;
    error_ = "";
    state_ = State::Streaming;
    backend_.advance(now);
    return issue(Command::Start);
}

void Session::abort()
{
    if (active())
        abortRequested_ = true;
}

void Session::tick(double now)
{
    if (!active())
        return;
    if (!backend_.connected() || !std::isfinite(now) || now < now_ || now - now_ > 1.0) {
        fault("Connection or clock lost; backend state unknown.");
        return;
    }
    now_ = now;
    backend_.advance(now);
    if (!pending_)
        return;
    if (now - sentAt_ >= 2.0) {
        fault("Acknowledgement timed out; backend state unknown.");
        return;
    }
    const auto reply = backend_.receive();
    if (!reply)
        return;
    const auto command = *std::exchange(pending_, {});
    if (!reply->accepted || reply->free < 0 || reply->free > backend_.capacity()) {
        fault("Rejected request or invalid backend reply.");
        return;
    }
    if (command == Command::Abort) {
        if (reply->mode == Mode::Idle && reply->free == backend_.capacity())
            state_ = State::Aborted;
        else
            fault("Abort not confirmed.");
        return;
    }
    if (abortRequested_) {
        issue(Command::Abort);
        return;
    }
    if (command == Command::Target)
        ++next_;
    if (state_ == State::Draining) {
        if (reply->mode == Mode::Idle && reply->free == backend_.capacity()
            && now >= trajectory_.back().time)
            state_ = State::Completed;
        else if (reply->mode == Mode::Ending)
            issue(Command::Status);
        else
            fault("Unexpected backend state while draining.");
        return;
    }
    if (reply->mode != Mode::Active) {
        fault("Unexpected backend mode.");
        return;
    }
    if (next_ == trajectory_.size()) {
        state_ = State::Draining;
        issue(Command::End);
    } else if (trajectory_[next_].time <= now) {
        fault("A target expired before it could be queued.");
    } else if (reply->free > 0 && trajectory_[next_].time - now <= limits_.lookahead) {
        issue(Command::Target, trajectory_[next_]);
    } else {
        issue(Command::Status);
    }
}

}
