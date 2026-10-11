#pragma once
#include <cmath>
struct Plan { bool day; int nextMinutes, sunrise, sunset, polar; };
inline int normalizeMinute(int minute) { return (minute % 1440 + 1440) % 1440; }
inline Plan fixedPlan(int now, int rise, int set)
{
    rise = normalizeMinute(rise); set = normalizeMinute(set);
    bool day = rise < set ? now >= rise && now < set : now >= rise || now < set;
    int nextRise = normalizeMinute(rise - now), nextSet = normalizeMinute(set - now);
    if (!nextRise) nextRise = 1440;
    if (!nextSet) nextSet = 1440;
    return {day, nextRise < nextSet ? nextRise : nextSet, rise, set, 0};
}
/* NOAA fractional-year approximation. Positive longitude is east.
   https://gml.noaa.gov/grad/solcalc/solareqns.PDF */
inline Plan solarPlan(int now, int dayOfYear, bool leap, double latitude, double longitude, int utcOffset,double zenith=90.833)
{
    const double pi = 3.14159265358979323846;
    double gamma = 2 * pi / (leap ? 366 : 365) * (dayOfYear - 1);
    double eq = 229.18 * (0.000075 + 0.001868 * std::cos(gamma) - 0.032077 * std::sin(gamma)
        - 0.014615 * std::cos(2*gamma) - 0.040849 * std::sin(2*gamma));
    double decl = 0.006918 - 0.399912 * std::cos(gamma) + 0.070257 * std::sin(gamma)
        - 0.006758 * std::cos(2*gamma) + 0.000907 * std::sin(2*gamma)
        - 0.002697 * std::cos(3*gamma) + 0.00148 * std::sin(3*gamma);
    double lat = latitude * pi / 180;
    double angle = (std::cos(zenith*pi/180) - std::sin(lat)*std::sin(decl)) / (std::cos(lat)*std::cos(decl));
    if (angle < -1 || angle > 1) return {angle < -1, 1440-now, -1, -1, angle < -1 ? 1 : -1};
    double halfDay = std::acos(angle) * 180 / pi * 4;
    double noon = 720 - 4 * longitude - eq + utcOffset;
    Plan result = fixedPlan(now, (int)std::lround(noon-halfDay), (int)std::lround(noon+halfDay));
    if (1440-now < result.nextMinutes) result.nextMinutes = 1440-now;
    return result;
}
/* First frame is dawn; the first half spans daylight, the remainder spans night.
   This maps every frame to its own phase instead of a two-image day/night toggle. */
inline int frameAt(int now,int count,int rise,int set,bool polar=false)
{
    if (count<2) return 0;
    if (polar) return now*count/1440;
    int daytime=normalizeMinute(set-rise);
    if (!daytime) return now*count/1440;
    int elapsed=normalizeMinute(now-rise), dayFrames=(count+1)/2;
    return elapsed<daytime ? elapsed*dayFrames/daytime :
        dayFrames+(elapsed-daytime)*(count-dayFrames)/(1440-daytime);
}
inline int nextFrameMinutes(int now,int count,int rise,int set,bool polar=false)
{
    int current=frameAt(now,count,rise,set,polar);
    for (int i=1;i<=1440;++i)
        if (frameAt(normalizeMinute(now+i),count,rise,set,polar)!=current) return i;
    return 1440;
}
