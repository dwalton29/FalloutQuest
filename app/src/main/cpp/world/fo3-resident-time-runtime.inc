// Render-thread bridge. Asset workers and performance/loading clocks stay raw.
fo3scene::ResidentPauseClock gFo3ResidentSteadyClock,gFo3ResidentXrClock;
std::chrono::steady_clock::time_point Fo3GameplayNow() noexcept {
    const auto raw=std::chrono::duration_cast<std::chrono::nanoseconds>(
        Fo3ResidentRawSteadyNow().time_since_epoch()).count();
    const auto adjusted=gFo3ResidentSteadyClock.Sample(raw,Fo3GameplayBlocked());
    return std::chrono::steady_clock::time_point{
        std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::nanoseconds{adjusted})};
}
int64_t Fo3GameplayXrTime(int64_t raw) noexcept {
    return gFo3ResidentXrClock.Sample(raw,Fo3GameplayBlocked());
}
void PauseFo3ResidentGameplayTime() noexcept {
    // Sample the exact acquisition point before latching the steady domain.
    // Fo3GameplayNow is first resumed only after the complete workflow unblocks.
    (void)Fo3GameplayNow();
    gFo3ResidentSteadyClock.Pause();gFo3ResidentXrClock.Pause();
}
