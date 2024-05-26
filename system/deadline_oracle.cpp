#include "deadline_oracle.h"

uint64_t DeadlineOracle::make_deadline_prediction()
{
    // tentatively set the deadline to be 50 millisecond later 
    return get_sys_clock()+50000000;
}