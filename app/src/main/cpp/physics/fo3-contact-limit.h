#pragma once
namespace fo3contact {
// Stop at the first contact along the curl, including a thin surface that the
// fully curled endpoint has already passed. Predicate receives fraction [0,1].
template<class Contact> float Limit(Contact contact){
    if(contact(0))return 0;
    float safe=0;
    for(int step=1;step<=8;++step){float end=step/8.f;
        if(contact(end)){float lo=safe,hi=end;for(int i=0;i<7;++i){float mid=(lo+hi)*.5f;if(contact(mid))hi=mid;else lo=mid;}return lo;}
        safe=end;
    }
    return 1;
}
}
