#ifndef SIRPG_PROFILER_HPP
#define SIRPG_PROFILER_HPP

// Tracy Profiler Integration Header
#ifdef SIRPG_ENABLE_TRACY
    #include <tracy/Tracy.hpp>
    #define SIRPG_PROFILE_ZONE() FrameMark; ZoneScoped
    #define SIRPG_PROFILE_ZONE_NAMED(name) ZoneScopedN(name)
    #define SIRPG_PROFILE_FRAME() FrameMark
#else
    #define SIRPG_PROFILE_ZONE()
    #define SIRPG_PROFILE_ZONE_NAMED(name)
    #define SIRPG_PROFILE_FRAME()
#endif

#endif // SIRPG_PROFILER_HPP
