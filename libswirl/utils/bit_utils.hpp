
#ifndef _reibit_utilts_hpp_
#define _reibit_utilts_hpp_
#include <stdint.h>

namespace bit_utils {
template <typename base_t>
static inline bool bit_isset(const base_t in,const base_t which) {
    return !!( in & ((base_t)1 << which));
}

template <typename base_t>
static inline void bit_on(base_t& in,const base_t which) {
    in |=  (base_t)1 << which;
}

template <typename base_t>
static inline void bit_off(base_t& in,const base_t which) {
    in &=  ~(  (base_t)1 << which );
}
 
 
template <typename base_t>
static inline void bit_set(base_t& in,const base_t which,const base_t state) {
    if (state == 0) {
        if (!bit_isset(in,which))
            return;
        bit_off(in,which);
        return;
    }

    if (bit_isset(in,which))
        return;
    bit_on(in,which);
} 

template <typename base_t>
static void dump_bits(const base_t in,const std::string& field) {
    const ssize_t num_bits = sizeof(in) << 3;
     
    printf("%s",field.c_str());
    for (ssize_t i = num_bits-1;i>=0;--i)
        printf("%d",!!(in &(1<<i)));

    printf("\n");
}
}
#endif