#pragma once
// Original Fallout 3 PACK PSDT schedule semantics, shared by resident and
// unloaded actor scheduling. Calendar months are GECK GameMonth 0..11.
#include "pipboy/fo3-pipboy-data.h"
#include <cmath>
namespace fo3schedule {
struct Calendar { int year=0,month=-1,day=0; };
inline bool Leap(int year) {return year%4==0&&(year%100!=0||year%400==0);}
inline int Days(int year,int month) {
  static constexpr int monthDays[12]={31,28,31,30,31,30,31,31,30,31,30,31};
  if(year<=0||month<0||month>=12)return 0;
  return monthDays[month]+(month==1&&Leap(year)?1:0);
}
inline bool Valid(Calendar c) {return c.day>=1&&c.day<=Days(c.year,c.month);}
inline int Weekday(Calendar c) {
  if(!Valid(c))return -1;
  // Gregorian Sunday=0, matching GECK GetDayOfWeek.
  static constexpr int offset[12]={0,3,2,5,0,3,5,1,4,6,2,4};
  const int month=c.month+1,y=c.year-(month<3);
  return (y+y/4-y/100+y/400+offset[c.month]+c.day)%7;
}
inline bool Active(const fo3pipdata::PackageSchedule& s,float hour,Calendar calendar={}) {
  if(!s.valid)return true;
  if(s.month!=-1||s.weekday!=-1||s.date!=0) {
    if(!Valid(calendar))return false;
    if(s.month!=-1&&s.month!=calendar.month)return false;
    if(s.weekday!=-1&&s.weekday!=Weekday(calendar))return false;
    if(s.date!=0&&s.date!=calendar.day)return false;
  }
  if(s.hour<0)return true;
  if(s.duration<=0||!std::isfinite(hour))return false;
  hour=std::fmod(hour,24.f);if(hour<0)hour+=24.f;
  const float end=float(s.hour)+float(s.duration);
  if(end<=24.f)return hour>=s.hour&&hour<end;
  return hour>=s.hour||hour<end-24.f;
}
} // namespace fo3schedule
