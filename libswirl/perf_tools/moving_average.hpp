//Author : Dimitrios Vlachos
//GIT : https://raw.githubusercontent.com/DimitrisVlachos/lib_moving_average

#ifndef _moving_average_hpp_
#define _moving_average_hpp_
#include <stdint.h>

template <typename base_t,const size_t factor>
class moving_average_c {
	private:
	static constexpr base_t ratio = (base_t)1;
	static constexpr base_t scalar_a = ratio / factor;
	static constexpr base_t scalar_b = ratio - scalar_a;
	base_t m_average;
	bool m_first;

	public:
	moving_average_c() : m_average((base_t)0) , m_first(true) {}
	moving_average_c(const base_t v) : m_average(v) , m_first(false) {}
	~moving_average_c() {}

	inline void reset() {
		m_first = true;
	}

	inline void reset(const base_t initial) {
		m_first = false;
		m_average = initial;
	}

	inline const base_t update(const base_t sample) {
		if (!m_first) { 
			return (m_average = (sample * scalar_a) + (m_average * scalar_b));
		}
        m_first = false;
		return (m_average = sample);
	}

	inline const base_t update_branchless(const base_t sample) { //make sure to call reset(val) first
		return (m_average = (sample * scalar_a) + (m_average * scalar_b));
	}

	inline const double get() const {
		return m_average;
	}
};

#endif
